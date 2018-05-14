#ifndef _HMAC_H
#define _HMAC_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
    void set_secret(const char *s, size_t l);
    char *hmac(const char *data);
    bool hmac_validate(const char *input, const char *expected);
#ifdef __cplusplus
}
#endif

#endif
