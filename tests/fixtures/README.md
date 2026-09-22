# MiniEAP SYSU 802.1X 真实网络测试报文集 (Test Fixtures)

本目录包含从中山大学（东校园至善园）锐捷 802.1X 接入交换机现场捕获的真实交互报文集。

所有数据包均已执行严格的数据清洗和隐私脱敏处理，去除了所有个人与网络敏感信息，可安全提交至开源代码仓库供单元测试与协议状态机仿真使用。

---

## 一、 脱敏规范说明

| 字段类别 | 原始现场值 | 脱敏后测试值 | 处理方式 |
| :--- | :--- | :--- | :--- |
| **客户端 MAC** | 已移除 | `02:00:00:00:00:01` | 替换为 IEEE 802 标准本地管理单播 MAC |
| **交换机 MAC** | 已移除 | `02:00:00:00:00:fe` | 替换为 IEEE 802 标准本地管理单播 MAC |
| **邻居主机 MAC** | 已移除 | `02:00:00:00:00:02` | 替换为测试单播 MAC |
| **802.1X 组播 MAC** | `01:80:c2:00:00:03` | `01:80:c2:00:00:03` | 保留标准 IEEE 802.1X PAE 组播地址 |
| **认证用户名** | 已移除（8 字节） | `testuser` (8 字节) | 等长虚拟用户名替换，保持报文长度和偏移一致 |
| **邻居用户名** | 已移除（10 字节） | `neighbor01` (10 字节) | 等长虚拟用户名替换 |
| **密码挑战计算散列** | 真实密码 MD5 响应值 | `01 23 45 67 89 ab cd ef fe dc ba 98 76 54 32 10` | 抹除根据真实密码计算出的 16 字节散列值，杜绝彩虹表碰撞与逆向 |

---

## 二、 报文文件列表

| 文件名 | 长度 | 发送源 -> 目标 | 说明 |
| :--- | :---: | :--- | :--- |
| `01_eapol_start.bin` | 18 字节 | Client -> `01:80:c2:00:00:03` | 客户端发出的 EAPOL-Start 启动帧 |
| `02_server_req_identity_bcast.bin` | 64 字节 | Server -> `01:80:c2:00:00:03` | 交换机发出的 EAP-Request/Identity 广播探测 |
| `03_client_resp_identity.bin` | 31 字节 | Client -> Server | 客户端响应 Identity（含用户名 `testuser`） |
| `04_server_req_md5_challenge_round1.bin` | 64 字节 | Server -> Client | 交换机第一阶段 MD5 挑战请求（ID=2） |
| `05_client_resp_md5_challenge_round1.bin` | 48 字节 | Client -> Server | 客户端响应第一阶段 MD5 挑战 |
| `06_server_req_md5_challenge_sysu_round2.bin` | 241 字节 | Server -> Client | 中大锐捷特定第二阶段挑战，携带服务器属性及帮助台提示信息 |
| `07_client_resp_md5_challenge_round2.bin` | 48 字节 | Client -> Server | 客户端响应第二阶段 MD5 挑战 |
| `08_server_eap_success.bin` | 64 字节 | Server -> Client | 交换机发出的 EAP-Success 认证成功报文（端口正式放行） |
| `09_server_req_identity_unicast_keepalive.bin` | 64 字节 | Server -> Client | 认证成功后，交换机定期向客户端单播发送的身份保活查询（ID=4） |
| `10_client_resp_identity_keepalive.bin` | 31 字节 | Client -> Server | 客户端对保活查询的应答帧（用于测试 SUCCESS 状态下不启动超时看门狗） |
| `11_foreign_neighbor_resp_identity.bin` | 60 字节 | Neighbor -> `01:80:c2:00:00:03` | 局域网内其他同学设备发出的组播响应（用于测试原始套接字报文过滤） |
| `12_server_eap_failure.bin` | 64 字节 | Server -> Client | 交换机发出的 EAP-Failure 认证失败/下线报文 |
| `13_server_req_md5_sysu_reauth_notify.bin` | 113 字节 | Server -> Client | 携带中大“前一次上网未正常下线”提示的特殊挑战报文 |
| `sysu_eap_auth_flow.pcap` | 3.2 KB | 混合流 | 包含上述全套序列的标准 PCAP 抓包文件（可直接用 Wireshark 打开） |
| `manifest.json` | - | - | 机器可读的报文元数据配置索引 |
