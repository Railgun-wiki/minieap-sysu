#ifndef _MINIEAP_RETRY_POLICY_H
#define _MINIEAP_RETRY_POLICY_H

static inline int eap_retry_limit_reached(int retry_count, int max_retries) {
    return max_retries > 0 && retry_count >= max_retries;
}

#endif
