#ifndef DP_FILMTABLE_CRYPT_H
#define DP_FILMTABLE_CRYPT_H

#ifdef __cplusplus
extern "C" {
#endif

void filmtable_crypto_init(void);
unsigned char filmtable_crypto_decrypt(unsigned char in);

#ifdef __cplusplus
}
#endif

#endif // DP_FILMTABLE_CRYPT_H
