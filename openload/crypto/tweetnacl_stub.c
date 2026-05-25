/*
 * tweetnacl 需要外部提供的 randombytes 实现.
 *
 * OpenLoad 仅用 ed25519 verify 路径 (crypto_sign_open), 不调 keypair/sign,
 * 所以这个符号在 verify 流程上永远不会被调用. linker -gc-sections 在大多数情况下
 * 能直接 GC 掉 tweetnacl 里 keypair/sign 的代码 -> 这个 stub 也会被 GC.
 *
 * 留 stub 是保险: 万一未来配置变化拉进了 sign/keypair, 至少 linker 不报
 * undefined reference. 现在所有调用都会进死循环, 立刻暴露 misuse.
 */
#include <stdint.h>

void randombytes(uint8_t *p, uint64_t n)
{
    (void)p; (void)n;
    /* 不应被调用. 死循环让看门狗复位, 比静默返回 0 安全. */
    while (1) { }
}
