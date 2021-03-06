/*
 * arch/ubicom32/crypto/aes_ubicom32.c
 *   Ubicom32 implementation of the AES Cipher Algorithm.
 *
 * (C) Copyright 2009, Ubicom, Inc.
 *
 * This file is part of the Ubicom32 Linux Kernel Port.
 *
 * The Ubicom32 Linux Kernel Port is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Ubicom32 Linux Kernel Port is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Ubicom32 Linux Kernel Port.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Ubicom32 implementation derived from (with many thanks):
 *   arch/m68knommu
 *   arch/blackfin
 *   arch/parisc
 */
#include <crypto/aes.h>
#include <crypto/algapi.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include "crypto_ubicom32.h"
#include <asm/linkage.h>

#if defined(CONFIG_UBICOM32_STOPWATCH_CRYPTO)
#define __STOPWATCH_USE__ 1
#endif
#include <asm/stopwatch.h>
#if defined(CONFIG_UBICOM32_STOPWATCH_CRYPTO)
/*
 *  Add one for 1024 transfer, and one for 512 transfer.
 */
DEFINE_STATIC_STOPWATCH_ARRAY(aes_watches, 3);
#endif

struct ubicom32_aes_ctx {
	u8 key[AES_MAX_KEY_SIZE];
	u32 ctrl;
	int key_len;
};

static inline void aes_hw_set_key(const u8 *key, u8 key_len)
{
	/*
	 * switch case has more overhead than 4 move.4 instructions, so just copy 256 bits
	 */
	SEC_SET_KEY_256(key);
}

static inline void aes_hw_set_iv(const u8 *iv)
{
	SEC_SET_IV_4W(iv);
}

static inline void aes_hw_cipher(u8 *out, const u8 *in)
{
	SEC_SET_INPUT_4W(in);

	asm volatile (                                                
	"	; start AES by writing 0x40(SECURITY_BASE)	\n\t" 
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_4)"(%[s_base]), #0		\n\t"
	"	pipe_flush 0                                    \n\t" 
	"							\n\t" 
	"	; wait for the module to calculate the output	\n\t" 
	"	btst "D(UBI32_SECURITY_SHA1_STAT)"(%[s_base]), #"D(UBI32_SECURITY_SHA1_STAT_BUSY_BIT)"		\n\t"
	"	jmpne.f .-4					\n\t" 
		:                                                             
		: [s_base]"a" (SEC_BASE)				                      
		: "cc"
	);

	SEC_GET_OUTPUT_4W(out);
}

static int __ocm_text aes_set_key(struct crypto_tfm *tfm, const u8 *in_key,
		       unsigned int key_len)
{
	struct ubicom32_aes_ctx *uctx = crypto_tfm_ctx(tfm);

	uctx->key_len = key_len;
	memcpy(uctx->key, in_key, key_len);

	/*
	 * leave out HASH_ALG (none = 0), CBC (no = 0), DIR (unknown) yet
	 */
	switch (uctx->key_len) {
	case 16:
		uctx->ctrl = SEC_KEY_128_BITS | SEC_ALG_AES;
		break;
	case 24:
		uctx->ctrl = SEC_KEY_192_BITS | SEC_ALG_AES;
		break;
	case 32:
		uctx->ctrl = SEC_KEY_256_BITS | SEC_ALG_AES;
		break;
	}

	return 0;
}

static inline void aes_cipher(struct crypto_tfm *tfm, u8 *out, const u8 *in, u32 extra_flags)
{
	const struct ubicom32_aes_ctx *uctx = crypto_tfm_ctx(tfm);
	unsigned long flags;

	STOPWATCH_DECLARE;

	flags = hw_crypto_lock();
	hw_crypto_check();

	STOPWATCH_START();

	hw_crypto_set_ctrl(uctx->ctrl | extra_flags);

	aes_hw_set_key(uctx->key, uctx->key_len);
	aes_hw_cipher(out, in);

	STOPWATCH_END(&aes_watches[0]);
	hw_crypto_unlock(flags);
}

static void aes_encrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	aes_cipher(tfm, out, in, SEC_DIR_ENCRYPT);
}

static void aes_decrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	aes_cipher(tfm, out, in, SEC_DIR_DECRYPT);
}

static struct crypto_alg aes_alg = {
	.cra_name		=	"aes",
	.cra_driver_name	=	"aes-ubicom32",
	.cra_priority		=	CRYPTO_UBICOM32_PRIORITY,
	.cra_flags		=	CRYPTO_ALG_TYPE_CIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct ubicom32_aes_ctx),
	.cra_alignmask		=	CRYPTO_UBICOM32_ALIGNMENT - 1,
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(aes_alg.cra_list),
	.cra_u			=	{
		.cipher = {
			.cia_min_keysize	=	AES_MIN_KEY_SIZE,
			.cia_max_keysize	=	AES_MAX_KEY_SIZE,
			.cia_setkey		=	aes_set_key,
			.cia_encrypt		=	aes_encrypt,
			.cia_decrypt		=	aes_decrypt,
		}
	}
};

static void __ocm_text ecb_aes_crypt_loop(u8 *out, u8 *in, unsigned int n)
{
	while (likely(n)) {
		aes_hw_cipher(out, in);
		out += AES_BLOCK_SIZE;
		in += AES_BLOCK_SIZE;
		n -= AES_BLOCK_SIZE;
	}
}

static int __ocm_text ecb_aes_crypt(struct blkcipher_desc *desc, struct scatterlist *dst,
			 struct scatterlist *src, unsigned int nbytes, u32 extra_flags)
{
	const struct ubicom32_aes_ctx *uctx = crypto_blkcipher_ctx(desc->tfm);
	int ret;
	struct blkcipher_walk walk;
	unsigned long flags;
	
	STOPWATCH_DECLARE;

	blkcipher_walk_init(&walk, dst, src, nbytes);
	ret = blkcipher_walk_virt(desc, &walk);
        if (ret) {
                return ret;
        }

	flags = hw_crypto_lock();
	hw_crypto_check();

	STOPWATCH_START();

        hw_crypto_set_ctrl(uctx->ctrl | extra_flags);
        aes_hw_set_key(uctx->key, uctx->key_len);

	while (likely((nbytes = walk.nbytes))) {
		/* only use complete blocks */
		unsigned int n = nbytes & ~(AES_BLOCK_SIZE - 1);
		u8 *out = walk.dst.virt.addr;
		u8 *in = walk.src.virt.addr;

		/* finish n/16 blocks */
		ecb_aes_crypt_loop(out, in, n);

		nbytes &= AES_BLOCK_SIZE - 1;
		ret = blkcipher_walk_done(desc, &walk, nbytes);
	}

	STOPWATCH_END(&aes_watches[1]);
	hw_crypto_unlock(flags);
	return ret;
}

static int ecb_aes_encrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	return ecb_aes_crypt(desc, dst, src, nbytes, SEC_DIR_ENCRYPT);
}

static int ecb_aes_decrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	return ecb_aes_crypt(desc, dst, src, nbytes, SEC_DIR_DECRYPT);
}

static struct crypto_alg ecb_aes_alg = {
	.cra_name		=	"ecb(aes)",
	.cra_driver_name	=	"ecb-aes-ubicom32",
	.cra_priority		=	CRYPTO_UBICOM32_COMPOSITE_PRIORITY,
	.cra_flags		=	CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct ubicom32_aes_ctx),
	.cra_alignmask          =       CRYPTO_UBICOM32_ALIGNMENT - 1,
	.cra_type		=	&crypto_blkcipher_type,
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(ecb_aes_alg.cra_list),
	.cra_u			=	{
		.blkcipher = {
			.min_keysize		=	AES_MIN_KEY_SIZE,
			.max_keysize		=	AES_MAX_KEY_SIZE,
			.setkey			=	aes_set_key,
			.encrypt		=	ecb_aes_encrypt,
			.decrypt		=	ecb_aes_decrypt,
		}
	}
};

#if CRYPTO_UBICOM32_LOOP_ASM
void __ocm_text cbc_aes_encrypt_loop(u8 *out, u8 *in, u8 *iv, unsigned int n)
{
	asm volatile (
	"; set init. iv 4w			\n\t"
	"	move.4 "D(UBI32_SECURITY_SEC_OUT_0)"(%[s_base]), 0x0(%[iv])	\n\t"
	"	move.4 "D(UBI32_SECURITY_SEC_OUT_1)"(%[s_base]), 0x4(%[iv])	\n\t"
	"	move.4 "D(UBI32_SECURITY_SEC_OUT_2)"(%[s_base]), 0x8(%[iv])	\n\t"
	"	move.4 "D(UBI32_SECURITY_SEC_OUT_3)"(%[s_base]), 0xc(%[iv])	\n\t"
	"					\n\t"
	"; we know n > 0, so we can always	\n\t"
	"; load the first block			\n\t"
	"; set input 4w				\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_0)"(%[s_base]), (%[in])4++	\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_1)"(%[s_base]), (%[in])4++	\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_2)"(%[s_base]), (%[in])4++	\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_3)"(%[s_base]), (%[in])4++	\n\t"
	"					\n\t"
	"; kickoff hw				\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_4)"(%[s_base]), #0		\n\t"
	"					\n\t"
	"; update n & flush			\n\t"
	"	pipe_flush 0			\n\t"
	"					\n\t"
	" 1:	add.4 %[n], #-16, %[n]		\n\t"
	"	jmpeq.f 5f			\n\t"
	"					\n\t"
	"; set input 4w	(2nd)			\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_0)"(%[s_base]), (%[in])4++	\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_1)"(%[s_base]), (%[in])4++	\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_2)"(%[s_base]), (%[in])4++	\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_3)"(%[s_base]), (%[in])4++	\n\t"
	"					\n\t"
	"					\n\t"
	"; wait for the previous output		\n\t"
	"	btst "D(UBI32_SECURITY_SHA1_STAT)"(%[s_base]), #"D(UBI32_SECURITY_SHA1_STAT_BUSY_BIT)"		\n\t"
	"	jmpne.f -4			\n\t"
	"					\n\t"
	"; read previous output			\n\t"
	"	move.4 (%[out])4++, "D(UBI32_SECURITY_SEC_OUT_0)"(%[s_base])	\n\t"
	"	move.4 (%[out])4++, "D(UBI32_SECURITY_SEC_OUT_1)"(%[s_base])	\n\t"
	"	move.4 (%[out])4++, "D(UBI32_SECURITY_SEC_OUT_2)"(%[s_base])	\n\t"
	"	move.4 (%[out])4++, "D(UBI32_SECURITY_SEC_OUT_3)"(%[s_base])	\n\t"
	"					\n\t"
	"; kick off hw for 2nd input		\n\t"
	"	move.4 "D(UBI32_SECURITY_SHA1_IN_4)"(%[s_base]), #0		\n\t"
	"					\n\t"
	"					\n\t"
	"; go back to loop			\n\t"
	"	jmpt 1b				\n\t"
	"					\n\t"
	"; wait for last output			\n\t" 
	" 5:	btst "D(UBI32_SECURITY_SHA1_STAT)"(%[s_base]), #"D(UBI32_SECURITY_SHA1_STAT_BUSY_BIT)"		\n\t"
        "       jmpne.f -4                      \n\t"
        "                                       \n\t"
	"; read last output			\n\t"
	"	move.4 (%[out])4++, "D(UBI32_SECURITY_SEC_OUT_0)"(%[s_base])	\n\t"
	"	move.4 (%[out])4++, "D(UBI32_SECURITY_SEC_OUT_1)"(%[s_base])	\n\t"
	"	move.4 (%[out])4++, "D(UBI32_SECURITY_SEC_OUT_2)"(%[s_base])	\n\t"
	"	move.4 (%[out])4++, "D(UBI32_SECURITY_SEC_OUT_3)"(%[s_base])	\n\t"
        "                                       \n\t"
	"; copy out iv				\n\t"
	"	move.4 (%[iv])4++, "D(UBI32_SECURITY_SEC_OUT_0)"(%[s_base])	\n\t"
	"	move.4 (%[iv])4++, "D(UBI32_SECURITY_SEC_OUT_1)"(%[s_base])	\n\t"
	"	move.4 (%[iv])4++, "D(UBI32_SECURITY_SEC_OUT_2)"(%[s_base])	\n\t"
	"	move.4 (%[iv])4++, "D(UBI32_SECURITY_SEC_OUT_3)"(%[s_base])	\n\t"
        "                                       \n\t"
		: [out]"+a" (out), [in]"+a" (in), [n]"+d" (n)
		: [s_base]"a" (SEC_BASE), [iv]"a" (iv)
		: "d15", "cc"
	);
}

#else

static void __ocm_text cbc_aes_encrypt_loop(u8 *out, u8 *in, u8 *iv, unsigned int n)
{
	aes_hw_set_iv(iv);
	while (likely(n)) {
		aes_hw_cipher(out, in);
		out += AES_BLOCK_SIZE;
		in += AES_BLOCK_SIZE;
		n -= AES_BLOCK_SIZE;
	}
	SEC_COPY_4W(iv, out - AES_BLOCK_SIZE);
}

#endif

static void __ocm_text cbc_aes_decrypt_loop(u8 *out, u8 *in, u8 *iv, unsigned int n)
{
        while (likely(n)) {
                aes_hw_set_iv(iv);
		SEC_COPY_4W(iv, in);
                aes_hw_cipher(out, in);
                out += AES_BLOCK_SIZE;
                in += AES_BLOCK_SIZE;
                n -= AES_BLOCK_SIZE;
        }
}

static int __ocm_text cbc_aes_crypt(struct blkcipher_desc *desc,
                           struct scatterlist *dst, struct scatterlist *src,
                           unsigned int nbytes, u32 extra_flags)
{
	struct ubicom32_aes_ctx *uctx = crypto_blkcipher_ctx(desc->tfm);
	int ret;
        struct blkcipher_walk walk;
	unsigned long flags;

	STOPWATCH_DECLARE;

        blkcipher_walk_init(&walk, dst, src, nbytes);
	ret = blkcipher_walk_virt(desc, &walk);
	if (unlikely(ret)) {
		return ret;
	}

        flags = hw_crypto_lock();
	hw_crypto_check();

	STOPWATCH_START();

        hw_crypto_set_ctrl(uctx->ctrl | extra_flags);
        aes_hw_set_key(uctx->key, uctx->key_len);

	while (likely((nbytes = walk.nbytes))) {
                /* only use complete blocks */
                unsigned int n = nbytes & ~(AES_BLOCK_SIZE - 1);
		if (likely(n)) {
	                u8 *out = walk.dst.virt.addr;
		        u8 *in = walk.src.virt.addr;

			if (extra_flags & SEC_DIR_ENCRYPT) {
				cbc_aes_encrypt_loop(out, in, walk.iv, n);
			} else {
				cbc_aes_decrypt_loop(out, in, walk.iv, n);
			}
		}

		nbytes &= AES_BLOCK_SIZE - 1;
                ret = blkcipher_walk_done(desc, &walk, nbytes);
	}
	STOPWATCH_END(&aes_watches[2]);
	hw_crypto_unlock(flags);

	return ret;
}

static int __ocm_text cbc_aes_encrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	return cbc_aes_crypt(desc, dst, src, nbytes, SEC_DIR_ENCRYPT | SEC_CBC_SET);
}

static int __ocm_text cbc_aes_decrypt(struct blkcipher_desc *desc,
			   struct scatterlist *dst, struct scatterlist *src,
			   unsigned int nbytes)
{
	return cbc_aes_crypt(desc, dst, src, nbytes, SEC_DIR_DECRYPT | SEC_CBC_SET);
}

static struct crypto_alg cbc_aes_alg = {
	.cra_name		=	"cbc(aes)",
	.cra_driver_name	=	"cbc-aes-ubicom32",
	.cra_priority		=	CRYPTO_UBICOM32_COMPOSITE_PRIORITY,
	.cra_flags		=	CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct ubicom32_aes_ctx),
	.cra_alignmask          =       CRYPTO_UBICOM32_ALIGNMENT - 1,
	.cra_type		=	&crypto_blkcipher_type,
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(cbc_aes_alg.cra_list),
	.cra_u			=	{
		.blkcipher = {
			.min_keysize		=	AES_MIN_KEY_SIZE,
			.max_keysize		=	AES_MAX_KEY_SIZE,
			.ivsize			=	AES_BLOCK_SIZE,
			.setkey			=	aes_set_key,
			.encrypt		=	cbc_aes_encrypt,
			.decrypt		=	cbc_aes_decrypt,
		}
	}
};

#if defined(CONFIG_UBICOM32_STOPWATCH_CRYPTO)
static const char *aes_names [3] = {
	"aes-cipher",
	"ecb-aes-crypt",
	"cbc-aes-crypt"
};

/*
 * aes_watch_show()
 *	Print out the min, avg, max values for each AES
 */
static int aes_watch_show(struct seq_file *p, void *v)
{
	int number = *((loff_t *) v);

	if (number == 0) {
		seq_puts(p, "\tmin\tavg\tmax\t(micro-seconds)\n");
	}

	stopwatch_show(&aes_watches[number], p, aes_names[number], STOPWATCH_SHOW_MICRO);
	return 0;
}

static int __init aes_watch_init(void)
{
	return stopwatch_register("aes", 3, aes_watch_show);
}

#endif

static int __init aes_init(void)
{
	int ret;

#if defined(CONFIG_UBICOM32_STOPWATCH_CRYPTO)
	aes_watch_init();
#endif
	hw_crypto_init();

	ret = crypto_register_alg(&aes_alg);
	if (ret)
		goto aes_err;

	ret = crypto_register_alg(&ecb_aes_alg);
	if (ret)
		goto ecb_aes_err;

	ret = crypto_register_alg(&cbc_aes_alg);
	if (ret)
		goto cbc_aes_err;

out:
	return ret;

cbc_aes_err:
	crypto_unregister_alg(&ecb_aes_alg);
ecb_aes_err:
	crypto_unregister_alg(&aes_alg);
aes_err:
	goto out;
}

static void __exit aes_fini(void)
{
	crypto_unregister_alg(&cbc_aes_alg);
	crypto_unregister_alg(&ecb_aes_alg);
	crypto_unregister_alg(&aes_alg);
}

module_init(aes_init);
module_exit(aes_fini);

MODULE_ALIAS("aes");

MODULE_DESCRIPTION("Rijndael (AES) Cipher Algorithm");
MODULE_LICENSE("GPL");
