#include "eap_state_machine.h"
#include "packet_builder.h"
#include "packet_plugin.h"
#include "packet_util.h"
#include "config.h"
#include "if_impl.h"
#include "logging.h"
#include "packet_util.h"
#include "minieap_common.h"
#include "eth_frame.h"
#include "net_util.h"
#include "sched_alarm.h"

#include <stdlib.h>

typedef struct _state_mach_priv {
    int state_last_count; // Number of timeouts occured in this state
    int auth_round; // Current authentication round
    int fail_count;
    int state_alarm_id;
    uint8_t local_mac[6];
    uint8_t server_mac[6];
    EAP_STATE state;
    ETH_EAP_FRAME* last_recv_frame;
    PACKET_BUILDER* packet_builder;
} STATE_MACH_PRIV;

static STATE_MACH_PRIV g_priv;

typedef struct _state_trans {
    EAP_STATE state;
    RESULT (*trans_func)(ETH_EAP_FRAME* frame);
} STATE_TRANSITION;

static RESULT trans_to_preparing(ETH_EAP_FRAME* frame);
static RESULT trans_to_start_sent(ETH_EAP_FRAME* frame);
static RESULT trans_to_identity_sent(ETH_EAP_FRAME* frame);
static RESULT trans_to_challenge_sent(ETH_EAP_FRAME* frame);
static RESULT trans_to_success(ETH_EAP_FRAME* frame);
static RESULT trans_to_failure(ETH_EAP_FRAME* frame);

static STATE_TRANSITION g_transition_table[] = {
    {EAP_STATE_UNKNOWN, NULL},
    {EAP_STATE_PREPARING, trans_to_preparing},
    {EAP_STATE_WAITING_FOR_CLIENT_START, NULL},
    {EAP_STATE_START_SENT, trans_to_start_sent},
    {EAP_STATE_WAITING_FOR_CLIENT_IDENTITY, NULL},
    {EAP_STATE_IDENTITY_SENT, trans_to_identity_sent},
    {EAP_STATE_WAITING_FOR_CLILENT_CHALLENGE, NULL},
    {EAP_STATE_CHALLENGE_SENT, trans_to_challenge_sent},
    {EAP_STATE_SUCCESS, trans_to_success},
    {EAP_STATE_FAILURE, trans_to_failure},
};

static const uint8_t BCAST_ADDR[6] = {0x01,0x80,0xc2,0x00,0x00,0x03};
static const uint8_t ETH_P_PAE_BYTES[2] = {0x88, 0x8e};

#define PRIV (&g_priv) // I like pointers!

static void disable_state_watchdog();

static void eap_state_machine_reset() {
    disable_state_watchdog();
    free_frame(&PRIV->last_recv_frame);
    PRIV->state_last_count = 0;
    PRIV->state = EAP_STATE_UNKNOWN; // If called by a transition func, this won't take effect
    PRIV->auth_round = 1;
    PRIV->fail_count = 0;
    memmove(PRIV->server_mac, BCAST_ADDR, sizeof(BCAST_ADDR));
}

RESULT eap_state_machine_init() {
    IF_IMPL* _if_impl = get_if_impl();
    char buf[IFNAMSIZ] = {0};

    _if_impl->get_ifname(_if_impl, buf, IFNAMSIZ);
    obtain_iface_mac(buf, PRIV->local_mac);

    eap_state_machine_reset();

    PRIV->packet_builder = packet_builder_get();

    return PRIV->packet_builder == NULL ? FAILURE : SUCCESS;
}

void eap_state_machine_destroy() {
    packet_builder_destroy();
    PRIV->packet_builder = NULL;
    free_frame(&PRIV->last_recv_frame);
}

static inline void set_outgoing_eth_fields(PACKET_BUILDER* builder) {
    builder->set_eth_field(builder, FIELD_DST_MAC, PRIV->server_mac);
    builder->set_eth_field(builder, FIELD_SRC_MAC, PRIV->local_mac);
    builder->set_eth_field(builder, FIELD_ETH_PROTO, ETH_P_PAE_BYTES);
}

/*
 * Packet senders
 *
 * Build the general response, call plugins to modify it, and send it.
 */
static RESULT state_mach_send_identity_response(ETH_EAP_FRAME* request) {
    uint8_t _buf[FRAME_BUF_SIZE] = {0};
    ETH_EAP_FRAME _response;
    IF_IMPL* _if_impl = get_if_impl();

    set_outgoing_eth_fields(PRIV->packet_builder);
    PRIV->packet_builder->set_eap_fields(PRIV->packet_builder,
                                EAP_PACKET, EAP_RESPONSE,
                                IDENTITY, request->header->eap_hdr.id[0],
                                get_eap_config());
    _response.actual_len = PRIV->packet_builder->build_packet(PRIV->packet_builder, _buf);
    _response.buffer_len = FRAME_BUF_SIZE;
    _response.content = _buf;

    if (IS_FAIL(packet_plugin_prepare_frame(&_response))) {
        PR_ERR("插件在准备发送 Response-Identity 包时出现错误");
        return FAILURE;
    }
    if (IS_FAIL(_if_impl->send_frame(_if_impl, &_response))) {
        PR_ERR("发送 Response-Identity 包时出现错误");
        return FAILURE;
    }
    return SUCCESS;
}

static RESULT state_mach_send_challenge_response(ETH_EAP_FRAME* request) {
    uint8_t _buf[FRAME_BUF_SIZE] = {0};
    ETH_EAP_FRAME _response;
    IF_IMPL* _if_impl = get_if_impl();

    set_outgoing_eth_fields(PRIV->packet_builder);
    PRIV->packet_builder->set_eap_fields(PRIV->packet_builder,
                                EAP_PACKET, EAP_RESPONSE,
                                MD5_CHALLENGE, request->header->eap_hdr.id[0],
                                get_eap_config());
    PRIV->packet_builder->set_eap_md5_seed(PRIV->packet_builder,
                                request->content + sizeof(FRAME_HEADER) + 1, /* 1 = sizeof(MD5-Value-Size) */
                                MD5_CHALLENGE_DIGEST_SIZE);
    _response.actual_len = PRIV->packet_builder->build_packet(PRIV->packet_builder, _buf);
    _response.buffer_len = FRAME_BUF_SIZE;
    _response.content = _buf;

    if (IS_FAIL(packet_plugin_prepare_frame(&_response))) {
        PR_ERR("插件在准备发送 Response-MD5-Challenge 包时出现错误");
        return FAILURE;
    }
    if (IS_FAIL(_if_impl->send_frame(_if_impl, &_response))) {
        PR_ERR("发送 Response-MD5-Challenge 包时出现错误");
        return FAILURE;
    }
    return SUCCESS;
}

static RESULT state_mach_send_eapol_simple(EAPOL_TYPE eapol_type) {
    uint8_t _buf[FRAME_BUF_SIZE] = {0};
    ETH_EAP_FRAME _response;
    IF_IMPL* _if_impl = get_if_impl();

    set_outgoing_eth_fields(PRIV->packet_builder);
    PRIV->packet_builder->set_eap_fields(PRIV->packet_builder,
                                eapol_type, 0,
                                0, 0,
                                NULL);
    _response.actual_len = PRIV->packet_builder->build_packet(PRIV->packet_builder, _buf);
    _response.buffer_len = FRAME_BUF_SIZE;
    _response.content = _buf;

    if (IS_FAIL(packet_plugin_prepare_frame(&_response))) {
        PR_ERR("插件在准备发送 %s 包时出现错误", str_eapol_type(eapol_type));
        return FAILURE;
    }
    if (IS_FAIL(_if_impl->send_frame(_if_impl, &_response))) {
        PR_ERR("发送 %s 包时出现错误", str_eapol_type(eapol_type));
        return FAILURE;
    }
    return SUCCESS;
}

static RESULT state_mach_process_success(ETH_EAP_FRAME* frame) {
    PROG_CONFIG* _cfg = get_program_config();
    if (PRIV->auth_round == _cfg->auth_round) {
        PR_INFO("认证成功");
        eap_state_machine_reset(); // Prepare for further use (e.g. re-auth after offline)
        return SUCCESS;
    } else {
        PR_INFO("第 %d 次认证成功，正在执行下一次认证", PRIV->auth_round);
        PRIV->fail_count = 0;
        packet_plugin_set_auth_round(++PRIV->auth_round);
        switch_to_state(EAP_STATE_START_SENT, frame); // Do not restart_auth or reset to keep auth_round
        return SUCCESS;
    }
}

static void restart_auth(void* unused) {
    eap_state_machine_reset();
    switch_to_state(EAP_STATE_START_SENT, NULL);
}

static const char* str_eap_state(EAP_STATE state) {
    switch (state) {
        case EAP_STATE_PREPARING:
            return "准备启动 (PREPARING)";
        case EAP_STATE_WAITING_FOR_CLIENT_START:
            return "等待客户端启动 (WAITING_START)";
        case EAP_STATE_START_SENT:
            return "寻找认证服务器 (START_SENT)";
        case EAP_STATE_WAITING_FOR_CLIENT_IDENTITY:
            return "等待客户端响应用户名 (WAITING_ID)";
        case EAP_STATE_IDENTITY_SENT:
            return "已响应用户名 (ID_SENT)";
        case EAP_STATE_WAITING_FOR_CLILENT_CHALLENGE:
            return "等待客户端响应密码 (WAITING_MD5)";
        case EAP_STATE_CHALLENGE_SENT:
            return "已响应密码验证 (MD5_SENT)";
        case EAP_STATE_SUCCESS:
            return "认证成功 (SUCCESS)";
        case EAP_STATE_FAILURE:
            return "认证失败 (FAILURE)";
        default:
            return "未知状态";
    }
}

static RESULT state_mach_process_failure(ETH_EAP_FRAME* frame) {
    PROG_CONFIG* _cfg = get_program_config();
    if (PRIV->state == EAP_STATE_SUCCESS) {
        /* Server forced us offline, not auth failing */
        if (_cfg->restart_on_logoff) {
            /* Wait for this state transition to FAILURE finish */
            PR_WARN("认证掉线（服务器发送下线通知），将在 1 秒后自动重新认证……");
            schedule_alarm(1, restart_auth, NULL);
        } else {
            PR_ERR("认证掉线（服务器发送下线通知），已配置禁止自动重连，正在退出……");
            exit(EXIT_FAILURE);
        }
    } else {
        /* Fail during auth */
        PRIV->fail_count++;
        PR_ERR("【认证失败】服务器拒绝了本次认证请求！");
        PR_ERR("【排障指南】请按以下步骤排查：\n"
               "  1. 账号或密码是否输入错误（检查大小写、空格）\n"
               "  2. 账号是否欠费、到期停机或在校园网后台被管理员冻结\n"
               "  3. 是否超出校园网允许的最大同时在线设备数\n"
               "  4. 是否绑定了特定网卡 MAC 地址（若绑定了电脑网卡，请在路由 WAN 口设置 MAC 克隆）");

        if (_cfg->max_failures > 0 && PRIV->fail_count >= _cfg->max_failures) {
            PR_ERR("已连续认证失败 %d 次，达到上限，为防止账号被系统锁定，正在退出……", PRIV->fail_count);
            exit(EXIT_FAILURE);
        } else {
            PR_WARN("认证失败 (第 %d 次)，将在 %d 秒后重新发起认证……",
                    PRIV->fail_count, _cfg->wait_after_fail_secs);
            schedule_alarm(_cfg->wait_after_fail_secs, restart_auth, NULL);
        }
    }
    return SUCCESS;
}

/*
 * This is the first function that will be notified on arrival of new frames.
 *
 * Dispatch the frame to plugins (to update their internal state,
 * preparing to modify the upcoming response frame)
 * and switch to next state (to send response)
 */
void eap_state_machine_recv_handler(ETH_EAP_FRAME* frame) {
    /* Keep a copy of the frame, since if_impl may not hold it */
    if (PRIV->last_recv_frame != NULL) {
        free_frame(&PRIV->last_recv_frame);
    }
    PRIV->last_recv_frame = frame_duplicate(frame);
    packet_plugin_on_frame_received(PRIV->last_recv_frame);

    EAPOL_TYPE _eapol_type = frame->header->eapol_hdr.type[0];
    if (_eapol_type == EAP_PACKET) {
        /* We don't want to handle other types here */
        EAP_TYPE _eap_type = frame->header->eap_hdr.type[0];
        EAP_CODE _eap_code = frame->header->eap_hdr.code[0];

        switch (_eap_code) {
            case EAP_REQUEST:
                /*
                 * Store server's MAC addr, do not use broadcast after.
                 */
                memmove(PRIV->server_mac, frame->header->eth_hdr.src_mac, 6);
                if (_eap_type == IDENTITY) {
                    switch_to_state(EAP_STATE_IDENTITY_SENT, frame);
                } else if (_eap_type == MD5_CHALLENGE) {
                    switch_to_state(EAP_STATE_CHALLENGE_SENT, frame);
                }
                break;
            case EAP_SUCCESS:
                switch_to_state(EAP_STATE_SUCCESS, frame);
                break;
            case EAP_FAILURE:
                switch_to_state(EAP_STATE_FAILURE, frame);
                break;
            default:
                break;
        }
    }
}

#define CFG_STAGE_TIMEOUT ((get_program_config())->stage_timeout)
/*
 * Re-transmit the response to last frame, in case the authentication server
 * stops responding.
 */
static void reset_state_watchdog();
static void state_watchdog(void* unused) {
    PROG_CONFIG* _cfg = get_program_config();
    PR_WARN("在阶段 [%s] 超时 (%d 秒) 未收到服务器响应，正在进行第 %d 次重试...",
            str_eap_state(PRIV->state),
            _cfg->stage_timeout,
            PRIV->state_last_count + 1);
    switch_to_state(PRIV->state, PRIV->last_recv_frame);
    reset_state_watchdog();
}

/*
 * Set a new watchdog for current state
 */
static void reset_state_watchdog() {
    unschedule_alarm(PRIV->state_alarm_id);
    PRIV->state_alarm_id = schedule_alarm(CFG_STAGE_TIMEOUT, state_watchdog, NULL);
}

static void disable_state_watchdog() {
    unschedule_alarm(PRIV->state_alarm_id);
    PRIV->state_alarm_id = 0;
}

/*
 * The transition functions
 *
 * Send appropriate authentication frame for specific state.
 * Besides that, deal with watchdog as well.
 */
static RESULT trans_to_preparing(ETH_EAP_FRAME* frame) {
    PR_INFO("========================");
    PR_INFO("MiniEAP " VERSION " 已启动");
    IF_IMPL* _if_impl = get_if_impl();
    RESULT ret = switch_to_state(EAP_STATE_START_SENT, frame);
    _if_impl->start_capture(_if_impl); // Blocking...
    return ret;
}

static RESULT trans_to_start_sent(ETH_EAP_FRAME* frame) {
    PR_INFO("正在查找认证服务器");
    return state_mach_send_eapol_simple(EAPOL_START);
}

static RESULT trans_to_identity_sent(ETH_EAP_FRAME* frame) {
    PR_INFO("正在回应用户名请求");
    return state_mach_send_identity_response(frame);
}

static RESULT trans_to_challenge_sent(ETH_EAP_FRAME* frame) {
    PR_INFO("正在回应密码请求");
    return state_mach_send_challenge_response(frame);
}

static RESULT trans_to_success(ETH_EAP_FRAME* frame) {
    disable_state_watchdog(); // Session finished,do not wait for new packets.
    return state_mach_process_success(frame);
}

static RESULT trans_to_failure(ETH_EAP_FRAME* frame) {
    disable_state_watchdog(); // Same as above.
    return state_mach_process_failure(frame);
}

/*
 * Look up the transition function for specific state, call it
 * and change the PRIV->state if it succeeds.
 *
 * Sets up watchdog when entering a new state (this watchdog will be
 * fed/reload when it barks, do not worry about that here). One can cancel
 * this watchdog in transition function if needed.
 */
RESULT switch_to_state(EAP_STATE state, ETH_EAP_FRAME* frame) {
    PROG_CONFIG* _cfg = get_program_config();

    if (PRIV->state == state) {
        /*
         * When max_retries > 0, check if we stayed in this state too long.
         * Setting max_retries <= 0 disables this limit (unlimited retries).
         */
        if (_cfg->max_retries > 0) {
            PRIV->state_last_count++;
            if (PRIV->state_last_count >= _cfg->max_retries) {
                PR_ERR("在阶段 [%s] 连续重试 %d 次均超时未收到响应，达到重试上限，正在退出！",
                       str_eap_state(PRIV->state), _cfg->max_retries);
                if (PRIV->state == EAP_STATE_START_SENT) {
                    PR_ERR("【排障指南】未收到来自锐捷认证服务器的应答。可能的原因：\n"
                           "  1. 路由器 WAN 口网线未插好或指示灯未亮\n"
                           "  2. 网络接口配置错误（当前配置接口: %s）\n"
                           "  3. 广播地址模式不匹配，可尝试在 LuCI/命令行配置中更改广播模式（例如 -a 1 私有组播/广播）",
                           _cfg->ifname ? _cfg->ifname : "未设置");
                } else if (PRIV->state == EAP_STATE_IDENTITY_SENT) {
                    PR_ERR("【排障指南】已发送用户名但未收到密码挑战请求。可能的原因：\n"
                           "  1. 账号不存在或后缀格式不正确\n"
                           "  2. 锐捷服务名称 (Service-Name) 与校园网不一致");
                } else if (PRIV->state == EAP_STATE_CHALLENGE_SENT) {
                    PR_ERR("【排障指南】已发送密码验证但未收到成功确认。可能的原因：\n"
                           "  1. 密码错误\n"
                           "  2. 锐捷版本号 (version-str) 不被服务器支持");
                }
                exit(EXIT_FAILURE);
            }
        }
    } else {
        /*
         * Reset watchdog before calling trans func
         * in case we need to cancel it there (e.g. after success)
         */
        PRIV->state_last_count = 0;
        reset_state_watchdog();
    }

    for (int i = 0; i < sizeof(g_transition_table) / sizeof(STATE_TRANSITION); ++i) {
        if (state == g_transition_table[i].state) {
            if (IS_FAIL(g_transition_table[i].trans_func(frame))) {
                exit(EXIT_FAILURE);
            } else {
                PRIV->state = state;
            }
            return SUCCESS;
        }
    }
    PR_WARN("%d 状态未定义", state);
    return SUCCESS;
}
