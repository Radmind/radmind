/*
 * OpenSSL Compatibility Shims for pre-1.1.0
 *
*/

#include <string.h>
#include <openssl/engine.h>
#if OPENSSL_VERSION_NUMBER < 0x10100000L

EVP_MD_CTX *EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX *ctx);

#endif // OLD OPENSSL <1.1.0
