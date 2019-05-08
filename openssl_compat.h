/*
 * OpenSSL Compatibility Shims for pre-1.1.0
 *
*/
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#ifndef RADMIND_OPENSSL_SHIMS
#define RADMIND_OPENSSL_SHIMS

#include <string.h>
#include <openssl/engine.h>

EVP_MD_CTX *EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX *ctx);

#endif // OPENSSL_RADMIND_SHIMS
#endif // OLD OPENSSL <1.1.0
