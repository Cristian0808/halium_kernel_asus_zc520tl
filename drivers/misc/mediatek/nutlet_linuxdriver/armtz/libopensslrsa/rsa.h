/* crypto/rsa/rsa.h */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#ifndef HEADER_RSA_H
# define HEADER_RSA_H

#include "bn.h"


//#define USE_PRE_KEYS
#ifdef USE_PRE_KEYS
#define SAVE_PRIME_NUM_2048  1000
#define SAVE_PRIME_NUM_1024  200
#endif


#define CRYPTO_LOCK_RSA                 9

/* Declared already in ossl_typ.h */
/* typedef struct rsa_st RSA; */
/* typedef struct rsa_meth_st RSA_METHOD; */
typedef struct rsa_st RSA;

struct rsa_st {
    /*
     * The first parameter is used to pickup errors where this is passed
     * instead of aEVP_PKEY, it is set to 0
     */
    int pad;
    long version;
    /* functional reference if 'meth' is ENGINE-provided */
   
    BIGNUM *n;
    BIGNUM *e;
    BIGNUM *d;
    BIGNUM *p;
    BIGNUM *q;
    BIGNUM *dmp1;
    BIGNUM *dmq1;
    BIGNUM *iqmp;
 
    int references;
    int flags;
    /* Used to cache montgomery values */
    BN_MONT_CTX *_method_mod_n;
    BN_MONT_CTX *_method_mod_p;
    BN_MONT_CTX *_method_mod_q;
  
};




#ifndef OPENSSL_RSA_MAX_MODULUS_BITS
#  define OPENSSL_RSA_MAX_MODULUS_BITS   16384
#endif

#ifndef OPENSSL_RSA_SMALL_MODULUS_BITS
#  define OPENSSL_RSA_SMALL_MODULUS_BITS 3072
#endif
#ifndef OPENSSL_RSA_MAX_PUBEXP_BITS

/* exponent limit enforced for "large" modulus only */
#  define OPENSSL_RSA_MAX_PUBEXP_BITS    64
#endif

# define RSA_3   0x3L
# define RSA_F4  0x10001L

# define RSA_METHOD_FLAG_NO_CHECK        0x0001/* don't check pub/private
                                                * match */

# define RSA_FLAG_CACHE_PUBLIC           0x0002
# define RSA_FLAG_CACHE_PRIVATE          0x0004
# define RSA_FLAG_BLINDING               0x0008
# define RSA_FLAG_THREAD_SAFE            0x0010

/*
 * new with 0.9.8f; the built-in RSA
 * implementation now uses constant time
 * operations by default in private key operations,
 * e.g., constant time modular exponentiation,
 * modular inverse without leaking branches,
 * division without leaking branches. This
 * flag disables these constant time
 * operations and results in faster RSA
 * private key operations.
 */
# define RSA_FLAG_NO_CONSTTIME           0x0100
# ifdef OPENSSL_USE_DEPRECATED
/* deprecated name for the flag*/
/*
 * new with 0.9.7h; the built-in RSA
 * implementation now uses constant time
 * modular exponentiation for secret exponents
 * by default. This flag causes the
 * faster variable sliding window method to
 * be used for all exponents.
 */
#  define RSA_FLAG_NO_EXP_CONSTTIME RSA_FLAG_NO_CONSTTIME
#endif

RSA *RSA_new(void);
int RSA_size(const RSA *rsa);

/* Deprecated version */
#ifndef OPENSSL_NO_DEPRECATED
RSA *RSA_generate_key(int bits, unsigned long e);
#endif                         /* !defined(OPENSSL_NO_DEPRECATED) */

/* New version */
int RSA_generate_key_ex(RSA *rsa, int bits, BIGNUM *e);

int RSA_check_key(const RSA *);
 
void RSA_free(RSA *r);



unsigned int openssl_rsa_gen_key(int keybits,unsigned char *rsa_n,unsigned int *nlen,
										unsigned char *rsa_d, unsigned int *dlen, int tt);
/*
 * If this flag is set the RSA method is FIPS compliant and can be used in
 * FIPS mode. This is set in the validated module method. If an application
 * sets this flag in its own methods it is its responsibility to ensure the
 * result is compliant.
 */

# define RSA_FLAG_FIPS_METHOD                    0x0400

/*
 * If this flag is set the operations normally disabled in FIPS mode are
 * permitted it is then the applications responsibility to ensure that the
 * usage is compliant.
 */

# define RSA_FLAG_NON_FIPS_ALLOW                 0x0400
/*
 * Application has decided PRNG is good enough to generate a key: don't
 * check.
 */
# define RSA_FLAG_CHECKED                        0x0800


/* Error codes for the RSA functions. */

/* Function codes. */
# define RSA_F_RSA_BUILTIN_KEYGEN                         129
# define RSA_F_RSA_CHECK_KEY                              123
# define RSA_F_RSA_GENERATE_KEY                           105
# define RSA_F_RSA_GENERATE_KEY_EX                        155
# define RSA_F_RSA_NEW_METHOD                             106

/* Reason codes. */
# define RSA_R_BAD_E_VALUE                                101
# define RSA_R_BAD_FIXED_HEADER_DECRYPT                   102
# define RSA_R_BAD_PAD_BYTE_COUNT                         103
# define RSA_R_DATA_GREATER_THAN_MOD_LEN                  108
# define RSA_R_DATA_TOO_LARGE                             109
# define RSA_R_DATA_TOO_SMALL                             111
# define RSA_R_DATA_TOO_SMALL_FOR_KEY_SIZE                122
# define RSA_R_DMP1_NOT_CONGRUENT_TO_D                    124
# define RSA_R_DMQ1_NOT_CONGRUENT_TO_D                    125
# define RSA_R_D_E_NOT_CONGRUENT_TO_1                     123
# define RSA_R_INVALID_HEADER                             137
# define RSA_R_INVALID_KEYBITS                            145
# define RSA_R_IQMP_NOT_INVERSE_OF_Q                      126
# define RSA_R_KEY_SIZE_TOO_SMALL                         120
# define RSA_R_MODULUS_TOO_LARGE                          105
# define RSA_R_NO_PUBLIC_EXPONENT                         140
# define RSA_R_N_DOES_NOT_EQUAL_P_Q                       127
# define RSA_R_P_NOT_PRIME                                128
# define RSA_R_Q_NOT_PRIME                                129
# define RSA_R_VALUE_MISSING                              147

#if 0
int expmod_test(void);
int mulmod_test(void);
#endif
int get_rsa_prime(int bits, unsigned char *key_n, int *nlen,
				unsigned char * key_d, int *dlen);

int ree_expmod(unsigned char *a, int alen,
				unsigned char * b, int blen,
				unsigned char * m, int mlen,
				unsigned char * r, int *rlen);
int ree_bn_is_prime( unsigned char *p, int plen);


#endif


