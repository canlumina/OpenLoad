/*
 * Host-side cross-check: pycryptodome ed25519 签的 image 能否被设备端
 * tweetnacl crypto_sign_open verify 通过. 加密 image 不解密, 用明文
 * payload 单独算 SHA-256, 然后 verify(pubkey, sha32, sig).
 *
 * 编译: gcc -I third_party/tweetnacl -I third_party/sha256 \
 *           tools/host_test_ed25519.c \
 *           third_party/tweetnacl/tweetnacl.c \
 *           third_party/sha256/sha256.c \
 *           -o tools/host_test_ed25519
 * 用:   tools/host_test_ed25519 <pubkey_hex_64> <plain_payload> <signed_image_with_sig_tail>
 */
#include "tweetnacl.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tweetnacl 期望 caller 提供 randombytes — verify 路径不调, stub 即可. */
void randombytes(unsigned char *p, unsigned long long n) { (void)p; (void)n; }

static int hex2bin(const char *h, unsigned char *b, size_t blen)
{
    if (strlen(h) != blen * 2) { return -1; }
    for (size_t i = 0; i < blen; ++i) {
        unsigned x; if (sscanf(h + 2*i, "%2x", &x) != 1) { return -1; }
        b[i] = (unsigned char)x;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s <pubkey_64hex> <plain_payload> <image_with_sig>\n", argv[0]);
        return 2;
    }

    unsigned char pubkey[32];
    if (hex2bin(argv[1], pubkey, 32) != 0) { fprintf(stderr, "pubkey hex err\n"); return 2; }

    /* 算 plain payload 的 SHA-256 (full 32 字节) */
    FILE *fp = fopen(argv[2], "rb"); if (!fp) { perror(argv[2]); return 2; }
    SHA256_CTX ctx; sha256_init(&ctx);
    unsigned char buf[1024]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) sha256_update(&ctx, buf, n);
    fclose(fp);
    unsigned char sha32[32]; sha256_final(&ctx, sha32);
    printf("plain SHA-256: "); for (int i=0;i<32;++i) printf("%02x", sha32[i]); printf("\n");

    /* 读 image_with_sig 末尾 64 字节作为 sig (image 末尾就是 sig). */
    FILE *fi = fopen(argv[3], "rb"); if (!fi) { perror(argv[3]); return 2; }
    fseek(fi, -64, SEEK_END);
    unsigned char sig[64];
    if (fread(sig, 1, 64, fi) != 64) { fprintf(stderr, "sig read err\n"); fclose(fi); return 2; }
    fclose(fi);
    printf("sig (64B)    : "); for (int i=0;i<16;++i) printf("%02x", sig[i]); printf("...\n");

    /* tweetnacl crypto_sign_open: sm = sig(64) || msg(32 = SHA), len = 96 */
    unsigned char sm[64 + 32];
    memcpy(sm, sig, 64);
    memcpy(sm + 64, sha32, 32);
    unsigned char m_out[64 + 32];
    unsigned long long mlen = 0;
    int rc = crypto_sign_open(m_out, &mlen, sm, sizeof(sm), pubkey);
    printf("verify       : %s (rc=%d, mlen=%llu)\n", rc == 0 ? "OK" : "FAIL", rc, mlen);
    return rc == 0 ? 0 : 1;
}
