#include <stdbool.h>
#include <string.h>

#include <openssl/hmac.h>

#include "hmac.h"

#define HEX_CHAR(c) \
    (c) == 0?  '0': \
    (c) == 1?  '1': \
    (c) == 2?  '2': \
    (c) == 3?  '3': \
    (c) == 4?  '4': \
    (c) == 5?  '5': \
    (c) == 6?  '6': \
    (c) == 7?  '7': \
    (c) == 8?  '8': \
    (c) == 9?  '9': \
    (c) == 10? 'a': \
    (c) == 11? 'b': \
    (c) == 12? 'c': \
    (c) == 13? 'd': \
    (c) == 14? 'e': \
             'f'
static char *as_hex(char *data, unsigned int len) {
    char *hex = malloc(2*len + 1);
    int i;
    for(i = 0; i < len; i++) {
        hex[2*i] = HEX_CHAR((data[i] & 0xf0) >> 4);
        hex[2*i + 1] = HEX_CHAR(data[i] & 0x0f);
    }
    hex[2*len] = '\0';
    return hex;
}

static const char *secret = NULL;
static size_t secret_len = 0;
void set_secret(const char *s, size_t l) {
    secret = s;
    secret_len = l;
}

char *hmac(const char *data) {
    char buf[EVP_MAX_MD_SIZE];
    unsigned int len;
    char *ret = HMAC(EVP_sha256(), secret, secret_len, data, strlen(data),
            buf, &len);
    if(!ret) return NULL;
    return as_hex(&buf[0], len);
}

bool hmac_validate(const char *input, const char *expected) {
    char *h = hmac(input);
    bool ret = !strcmp(h, expected);
    free(h);
    return ret;
}
