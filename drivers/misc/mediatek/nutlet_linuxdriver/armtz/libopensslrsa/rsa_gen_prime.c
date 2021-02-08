/* test vectors from p1ovect1.txt */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


//#include "e_os.h"

#include "err.h"
#include "bn.h"
#include <time.h>
#include "rsa.h"


int RSA_generate_prime( int bits, unsigned char *p, int *plen,
				unsigned char * q, int *qlen)
{
	BIGNUM *bn = NULL;
	BIGNUM *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL;
	BIGNUM *bn_p = NULL, *bn_q = NULL;
	int ret = 1;
	int bitsp, bitsq;
	BN_CTX *ctx = NULL;
	//unsigned long stime ,etime;
				 
	if (!(bn = BN_new()))
			goto end;
	if (!BN_set_word(bn, RSA_F4))
			goto end;	
			
	ctx = BN_CTX_new();
	if (ctx == NULL)
		 goto end;
	BN_CTX_start(ctx);
	r0 = BN_CTX_get(ctx);
	r1 = BN_CTX_get(ctx);
	r2 = BN_CTX_get(ctx);
	r3 = BN_CTX_get(ctx);
	if (r3 == NULL)
		 goto end;
	bitsp = bits;
	bitsq = bits;
			
	/* We need the RSA components non-NULL */

	if (!(bn_p = BN_new()))
		goto end;
	if (!(bn_q = BN_new()))
		goto end;
				
			
	/* generate p and q */			
	for (;;) {
			
		if (!BN_generate_prime_ex(bn_p, bitsp, 1, NULL))
			goto end;
		if (!BN_sub(r2, bn_p, BN_value_one()))
			goto end;
		if (!BN_gcd(r1, r2, bn, ctx))
			goto end;
		if (BN_is_one(r1))
			break;
	}
			   		
	for (;;) {
		/*
		 * When generating ridiculously small keys, we can get stuck
		 * continually regenerating the same prime values. Check for this and
		 * bail if it happens 3 times.
		 */
			 
		unsigned int degenerate = 0;
		do {
			if (!BN_generate_prime_ex(bn_q, bitsq, 1, NULL))
			 	goto end;
			} while ((BN_cmp(bn_p, bn_q) == 0) && (++degenerate < 3));
			if (degenerate == 3) {
					 //RSAerr(RSA_F_RSA_BUILTIN_KEYGEN, RSA_R_KEY_SIZE_TOO_SMALL);
				 goto end;
			}
			if (!BN_sub(r2, bn_q, BN_value_one()))
				goto end;
			if (!BN_gcd(r1, r2, bn, ctx))
				goto end;
			if (BN_is_one(r1))
				break;
			  
	}
			
	if (BN_cmp(bn_p, bn_q) < 0) 
	{
		r0 = bn_p;
		bn_p = bn_q;
		bn_q = r0;
	}
	*plen = BN_bn2bin(bn_p,p);
	*qlen = BN_bn2bin(bn_q,q);
	ret = 0;
 end:
 	if (bn != NULL)
		BN_free(bn);
	if (bn_p != NULL)
		BN_free(bn_p);
	if (bn_q != NULL)
		BN_free(bn_q);
	
	if (ctx != NULL)
	{
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}
	
	return ret;
}

//return 0 :SUCCESS
int get_rsa_prime(int bits, unsigned char *p, int *plen,
				unsigned char * q, int *qlen)
{

	//srand(time(0));
	return RSA_generate_prime(bits, p, plen, q, qlen);
}
//return 0 :SUCCESS
int ree_expmod(unsigned char *a, int alen,
				unsigned char * b, int blen,
				unsigned char * m, int mlen,
				unsigned char * r, int *rlen)
{
	int ret = 1;
	BIGNUM *bn_a = NULL,*bn_b = NULL,*bn_m = NULL,*bn_r = NULL;
	BN_CTX *ctx = NULL;
	
	ctx = BN_CTX_new();
	if (ctx == NULL)
		return 1;
	 
	if (!(bn_a= BN_new()))
		goto end;
	if (!(bn_b = BN_new()))
		goto end;
	if (!(bn_m = BN_new()))
		goto end;
	if (!(bn_r = BN_new()))
		goto end;
	
	if( !BN_bin2bn(a, alen, bn_a))
		goto end;
	if( !BN_bin2bn(b, blen, bn_b))
		goto end;
	if( !BN_bin2bn(m, mlen, bn_m))
		goto end;
	
//	if( !BN_mod_exp(bn_r, bn_a, bn_b, bn_m, NULL))
//		goto end;
	if( !BN_mod_exp(bn_r, bn_a, bn_b, bn_m, ctx))
			goto end;


	*rlen = BN_bn2bin(bn_r, r);

	ret = 0;
	end:
	if (bn_a)
		BN_free(bn_a);
	if (bn_b)
		BN_free(bn_b);
	if (bn_m)
		BN_free(bn_m);
	if (bn_r)
		BN_free(bn_r);
	if (ctx)
		BN_CTX_free(ctx);
	return ret ;
}

//return 0 :SUCCESS
int ree_bn_is_prime( unsigned char *p, int plen)
{
	BIGNUM *a = NULL;
	int bits ;
	int checks;
	BN_CTX *ctx = NULL;
	int ret = -1;

	 if (!(a = BN_new()))
		goto err;
	
	if( !BN_bin2bn(p, plen, a))
		goto err;
	
	bits = plen * 8;//BN_num_bits((const BIGNUM *) a);
	checks = BN_prime_checks_for_size(bits);
	
	ctx = BN_CTX_new();
	if (ctx == NULL)
		 goto err;
	BN_CTX_start(ctx);
	 
	ret = BN_is_prime_fasttest_ex((const BIGNUM *)a, checks, ctx,
								1);
		
	err:
	if (a)
		BN_free(a);
    if (ctx != NULL) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
   
	return ret;
}
                            

#if 0
int main()
{
	unsigned char p[256] = {0};
	unsigned char q[256] = {0};
	int plen = sizeof(p);
	int qlen = sizeof(q);
	int i;

	int ret = get_rsa_prime(1024, p, &plen, q, &qlen);
	if(ret == 0)
	{
	printf("\nP:\n");
	for( i = 0; i < plen; i++)
		printf("%02x", p[i]);
	printf("\nq:\n");
	for( i = 0; i < qlen; i++)
		printf("%02x", q[i]);
	printf("\n");
	}
	
	return 1;
}
#endif
