#include <assert.h>

#include "retry_policy.h"

int main(void) {
    assert(!eap_retry_limit_reached(0, 1));
    assert(eap_retry_limit_reached(1, 1));

    assert(!eap_retry_limit_reached(0, 3));
    assert(!eap_retry_limit_reached(1, 3));
    assert(!eap_retry_limit_reached(2, 3));
    assert(eap_retry_limit_reached(3, 3));

    assert(!eap_retry_limit_reached(100, 0));
    assert(!eap_retry_limit_reached(100, -1));
    return 0;
}
