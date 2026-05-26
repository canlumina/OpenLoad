/*
 * Host-side cross-check: 用设备端同一份 Brad Conte sha256.c 算文件 SHA-256,
 * 跟 Python hashlib + image_tool 写头的值对比.
 *
 * 编译: gcc -I third_party/sha256 tools/host_test_sha.c \
 *           third_party/sha256/sha256.c -o tools/host_test_sha
 * 用:   tools/host_test_sha app.bin
 */
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s <file>\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }

    SHA256_CTX ctx; sha256_init(&ctx);
    BYTE buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha256_update(&ctx, buf, n);
    }
    fclose(f);

    BYTE hash[32];
    sha256_final(&ctx, hash);
    for (int i = 0; i < 32; ++i) { printf("%02x", hash[i]); }
    printf("\n");
    return 0;
}
