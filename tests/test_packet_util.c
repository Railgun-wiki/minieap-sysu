#include <assert.h>
#include <stdint.h>

#include "packet_util.h"

int main(void) {
    uint8_t storage[22] = {0};
    ETH_EAP_FRAME source = {
        .actual_len = sizeof(storage),
        .buffer_len = sizeof(storage),
        .content = storage
    };

    ETH_EAP_FRAME* copy = frame_duplicate(&source);
    assert(copy != NULL);
    assert(copy->actual_len == sizeof(storage));
    assert(copy->buffer_len >= sizeof(FRAME_HEADER));
    assert(copy->content[sizeof(storage)] == 0);
    free_frame(&copy);
    assert(copy == NULL);
    return 0;
}
