#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eap_frame_validation.h"

typedef struct _fixture_case {
    const char* file;
    size_t length;
    int accepted_by_validator;
    int from_authenticator;
} FIXTURE_CASE;

static const FIXTURE_CASE FIXTURES[] = {
    {"01_eapol_start.bin", 18, 0, 0},
    {"02_server_req_identity_bcast.bin", 64, 1, 1},
    {"03_client_resp_identity.bin", 31, 0, 0},
    {"04_server_req_md5_challenge_round1.bin", 64, 1, 1},
    {"05_client_resp_md5_challenge_round1.bin", 48, 0, 0},
    {"06_server_req_md5_challenge_sysu_round2.bin", 241, 1, 1},
    {"07_client_resp_md5_challenge_round2.bin", 48, 0, 0},
    {"08_server_eap_success.bin", 64, 1, 1},
    {"09_server_req_identity_unicast_keepalive.bin", 64, 1, 1},
    {"10_client_resp_identity_keepalive.bin", 31, 0, 0},
    {"11_foreign_neighbor_resp_identity.bin", 60, 0, 0},
    {"12_server_eap_failure.bin", 64, 1, 1},
    {"13_server_req_md5_sysu_reauth_notify.bin", 113, 1, 1},
};

static uint8_t* load_fixture(const char* name, size_t expected_length) {
    char path[256];
    snprintf(path, sizeof(path), "fixtures/%s", name);
    FILE* fp = fopen(path, "rb");
    assert(fp != NULL);

    uint8_t* data = malloc(expected_length);
    assert(data != NULL);
    assert(fread(data, 1, expected_length, fp) == expected_length);
    assert(fgetc(fp) == EOF);
    assert(fclose(fp) == 0);
    return data;
}

static uint32_t read_le32(const uint8_t value[4]) {
    return (uint32_t)value[0] |
           ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) |
           ((uint32_t)value[3] << 24);
}

static void fill_base_frame(uint8_t* storage, size_t size, EAP_CODE code,
                            uint16_t eap_len) {
    memset(storage, 0, size);
    storage[EAPOL_TYPE_OFFSET] = EAP_PACKET;
    storage[EAPOL_LENGTH_OFFSET] = (uint8_t)(eap_len >> 8);
    storage[EAPOL_LENGTH_OFFSET + 1] = (uint8_t)eap_len;
    storage[EAP_CODE_OFFSET] = (uint8_t)code;
    storage[EAP_LENGTH_OFFSET] = (uint8_t)(eap_len >> 8);
    storage[EAP_LENGTH_OFFSET + 1] = (uint8_t)eap_len;
}

static void test_success_and_failure_lengths(void) {
    uint8_t storage[22];
    ETH_EAP_FRAME frame = {.actual_len = sizeof(storage),
                           .buffer_len = sizeof(storage),
                           .content = storage};

    fill_base_frame(storage, sizeof(storage), EAP_SUCCESS, 4);
    assert(eap_frame_has_valid_lengths(&frame));

    storage[EAP_LENGTH_OFFSET] = 0xff;
    storage[EAP_LENGTH_OFFSET + 1] = 0xff;
    assert(eap_frame_has_valid_lengths(&frame));

    fill_base_frame(storage, sizeof(storage), EAP_FAILURE, 4);
    assert(eap_frame_has_valid_lengths(&frame));

    frame.actual_len--;
    assert(!eap_frame_has_valid_lengths(&frame));
}

static void test_md5_challenge_lengths(void) {
    uint8_t storage[40];
    ETH_EAP_FRAME frame = {.actual_len = sizeof(storage),
                           .buffer_len = sizeof(storage),
                           .content = storage};

    fill_base_frame(storage, sizeof(storage), EAP_REQUEST, 22);
    storage[EAP_TYPE_OFFSET] = MD5_CHALLENGE;
    storage[EAP_MD5_VALUE_SIZE_OFFSET] = EAP_MD5_VALUE_SIZE;
    assert(eap_frame_has_valid_lengths(&frame));

    frame.actual_len = EAP_MD5_VALUE_SIZE_OFFSET;
    assert(!eap_frame_has_valid_lengths(&frame));
    frame.actual_len = sizeof(storage);

    storage[EAP_MD5_VALUE_SIZE_OFFSET] = EAP_MD5_VALUE_SIZE - 1;
    assert(!eap_frame_has_valid_lengths(&frame));
}

static void test_declared_length_cannot_exceed_capture(void) {
    uint8_t storage[23];
    ETH_EAP_FRAME frame = {.actual_len = sizeof(storage),
                           .buffer_len = sizeof(storage),
                           .content = storage};

    fill_base_frame(storage, sizeof(storage), EAP_REQUEST, 64);
    storage[EAP_TYPE_OFFSET] = IDENTITY;
    assert(!eap_frame_has_valid_lengths(&frame));
}

static void test_authenticator_source_binding(void) {
    uint8_t storage[23];
    ETH_EAP_FRAME frame = {.actual_len = sizeof(storage),
                           .buffer_len = sizeof(storage),
                           .content = storage};
    const uint8_t server[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    const uint8_t foreign[6] = {0x00, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};

    fill_base_frame(storage, sizeof(storage), EAP_REQUEST, 5);
    memcpy(storage + EAP_ETH_SOURCE_OFFSET, server, 6);
    assert(eap_frame_source_is_expected(&frame, server, 0));
    assert(eap_frame_source_is_expected(&frame, server, 1));

    memcpy(storage + EAP_ETH_SOURCE_OFFSET, foreign, 6);
    assert(!eap_frame_source_is_expected(&frame, server, 1));

    fill_base_frame(storage, sizeof(storage), EAP_SUCCESS, 4);
    assert(!eap_frame_source_is_expected(&frame, server, 0));
}

static void test_sysu_capture_fixtures(void) {
    const uint8_t server[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0xfe};

    for (size_t i = 0; i < sizeof(FIXTURES) / sizeof(FIXTURES[0]); ++i) {
        const FIXTURE_CASE* fixture = &FIXTURES[i];
        uint8_t* data = load_fixture(fixture->file, fixture->length);
        ETH_EAP_FRAME frame = {
            .actual_len = fixture->length,
            .buffer_len = fixture->length,
            .content = data
        };

        assert(eap_frame_has_valid_lengths(&frame) == fixture->accepted_by_validator);
        if (fixture->accepted_by_validator) {
            assert(eap_frame_source_is_expected(&frame, server, 1) ==
                   fixture->from_authenticator);
        }
        free(data);
    }
}

static void test_pcap_matches_binary_fixtures(void) {
    FILE* fp = fopen("fixtures/sysu_eap_auth_flow.pcap", "rb");
    assert(fp != NULL);

    uint8_t global_header[24];
    assert(fread(global_header, 1, sizeof(global_header), fp) == sizeof(global_header));
    assert(read_le32(global_header) == 0xa1b2c3d4);
    assert(read_le32(global_header + 20) == 1); /* LINKTYPE_ETHERNET */

    for (size_t i = 0; i < sizeof(FIXTURES) / sizeof(FIXTURES[0]); ++i) {
        uint8_t packet_header[16];
        assert(fread(packet_header, 1, sizeof(packet_header), fp) == sizeof(packet_header));
        assert(read_le32(packet_header + 8) == FIXTURES[i].length);
        assert(read_le32(packet_header + 12) == FIXTURES[i].length);

        uint8_t* captured = malloc(FIXTURES[i].length);
        assert(captured != NULL);
        assert(fread(captured, 1, FIXTURES[i].length, fp) == FIXTURES[i].length);
        uint8_t* fixture = load_fixture(FIXTURES[i].file, FIXTURES[i].length);
        assert(memcmp(captured, fixture, FIXTURES[i].length) == 0);
        free(fixture);
        free(captured);
    }

    assert(fgetc(fp) == EOF);
    assert(fclose(fp) == 0);
}

int main(void) {
    test_success_and_failure_lengths();
    test_md5_challenge_lengths();
    test_declared_length_cannot_exceed_capture();
    test_authenticator_source_binding();
    test_sysu_capture_fixtures();
    test_pcap_matches_binary_fixtures();
    return 0;
}
