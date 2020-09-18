/*
 * OpenSSL Compatibility Shims for pre-1.1.0
 *
*/

#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#include <string.h>
#include <openssl/engine.h>

EVP_MD_CTX *EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX *ctx);

#endif // OLD OPENSSL <1.1.0
