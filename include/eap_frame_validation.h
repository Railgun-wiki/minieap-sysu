#ifndef _MINIEAP_EAP_FRAME_VALIDATION_H
#define _MINIEAP_EAP_FRAME_VALIDATION_H

#include "eth_frame.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define EAP_MD5_VALUE_SIZE 16

enum {
    EAP_ETH_DEST_OFFSET = offsetof(FRAME_HEADER, eth_hdr.dest_mac),
    EAP_ETH_SOURCE_OFFSET = offsetof(FRAME_HEADER, eth_hdr.src_mac),
    EAPOL_TYPE_OFFSET = offsetof(FRAME_HEADER, eapol_hdr.type),
    EAPOL_LENGTH_OFFSET = offsetof(FRAME_HEADER, eapol_hdr.len),
    EAP_CODE_OFFSET = offsetof(FRAME_HEADER, eap_hdr.code),
    EAP_LENGTH_OFFSET = offsetof(FRAME_HEADER, eap_hdr.len),
    EAP_TYPE_OFFSET = offsetof(FRAME_HEADER, eap_hdr.type),
    EAP_MD5_VALUE_SIZE_OFFSET = sizeof(FRAME_HEADER)
};

static inline uint16_t eap_read_be16(const uint8_t value[2]) {
    return (uint16_t)(((uint16_t)value[0] << 8) | value[1]);
}

/*
 * Validate every field that the state machine reads. Ruijie properties may
 * follow the declared EAPOL payload, so trailing bytes are intentionally
 * allowed while truncated declared payloads are rejected.
 */
static inline int eap_frame_has_valid_lengths(const ETH_EAP_FRAME* frame) {
    const size_t eapol_offset = sizeof(ETHERNET_HEADER);
    const size_t eap_offset = eapol_offset + sizeof(EAPOL_HEADER);
    const size_t eap_min_len = sizeof(EAP_HEADER) - sizeof(((EAP_HEADER*)0)->type);

    if (frame == NULL || frame->content == NULL ||
        frame->actual_len < eap_offset + eap_min_len) {
        return 0;
    }
    if (frame->content[EAPOL_TYPE_OFFSET] != EAP_PACKET) {
        return 0;
    }

    const size_t eapol_len = eap_read_be16(frame->content + EAPOL_LENGTH_OFFSET);
    if (eapol_len < eap_min_len || eap_offset + eapol_len > frame->actual_len) {
        return 0;
    }

    const EAP_CODE code = frame->content[EAP_CODE_OFFSET];
    if (code == EAP_SUCCESS || code == EAP_FAILURE) {
        /* Some Ruijie result frames carry an unreliable EAP length field. */
        return 1;
    }

    const size_t eap_len = eap_read_be16(frame->content + EAP_LENGTH_OFFSET);
    if (code != EAP_REQUEST || eap_len < sizeof(EAP_HEADER)) {
        return 0;
    }
    if (eap_len > eapol_len) {
        return 0;
    }

    if (frame->content[EAP_TYPE_OFFSET] == MD5_CHALLENGE) {
        const size_t value_size_offset = EAP_MD5_VALUE_SIZE_OFFSET;
        if (eap_len < sizeof(EAP_HEADER) + 1 ||
            value_size_offset >= frame->actual_len) {
            return 0;
        }
        const size_t value_size = frame->content[value_size_offset];
        if (value_size != EAP_MD5_VALUE_SIZE ||
            eap_len < sizeof(EAP_HEADER) + 1 + value_size) {
            return 0;
        }
    }

    return 1;
}

static inline const uint8_t* eap_frame_dest_mac(const ETH_EAP_FRAME* frame) {
    return frame->content + EAP_ETH_DEST_OFFSET;
}

static inline const uint8_t* eap_frame_source_mac(const ETH_EAP_FRAME* frame) {
    return frame->content + EAP_ETH_SOURCE_OFFSET;
}

static inline EAP_CODE eap_frame_code(const ETH_EAP_FRAME* frame) {
    return (EAP_CODE)frame->content[EAP_CODE_OFFSET];
}

static inline EAP_TYPE eap_frame_type(const ETH_EAP_FRAME* frame) {
    return (EAP_TYPE)frame->content[EAP_TYPE_OFFSET];
}

static inline int eap_frame_source_is_expected(const ETH_EAP_FRAME* frame,
                                                const uint8_t server_mac[6],
                                                int server_known) {
    if (frame == NULL || frame->content == NULL || server_mac == NULL) {
        return 0;
    }
    if (server_known) {
        return memcmp(eap_frame_source_mac(frame), server_mac, 6) == 0;
    }
    return eap_frame_code(frame) == EAP_REQUEST;
}

#endif
