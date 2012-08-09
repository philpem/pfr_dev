#include "dp_filmtable_crypt.h"

/****************************************************************************
 * FILM TABLE CRYPTO
 ****************************************************************************/
/// Current keystream value
static unsigned char filmtable_keystream = 0;

/**
 * @brief Filmtable crypto -- bit permutation
 * Permutes bits as follows:
 *   * Bits 0 and 7 are swapped
 *   * Bits 1 and 2 pass straight through
 *   * Bits 3 amd 4 are swapped
 *   * Bit  5 is XORed with bit 1
 *   * Bit  6 is XORed with bit 2
 */
static unsigned char filmtable_crypto_bitperm(const unsigned char in)
{
	return
		((in & 0x01) << 7)	|	// Swap bits 0 and 7
		((in & 0x80) >> 7)	|
		 (in & 0x06)		|	// Bits 1 and 2 pass through
		((in & 0x08) << 1)	|	// Swap bits 3 and 4
		((in & 0x10) >> 1)	|
		((in & 0x60) ^ ((in & 0x06) << 4));	// XOR {b5,b6} with {b2,b1}
}

/// get next keystream byte
static unsigned char filmtable_crypto_next(void)
{
	unsigned char x = filmtable_keystream;
	filmtable_keystream = ((13 * filmtable_keystream) + 7);

	return x;
}

/**
 * @brief Initialise the filmtable decryption code
 */
void filmtable_crypto_init(void)
{
	filmtable_keystream = 0x35;
}

/**
 * @brief Decrypt a byte of the filmtable
 */
unsigned char filmtable_crypto_decrypt(unsigned char in)
{
	return filmtable_crypto_bitperm(filmtable_crypto_next() ^ in);
}
