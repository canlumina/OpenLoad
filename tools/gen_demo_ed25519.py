"""Generate demo Ed25519 keypair for OpenLoad M4-2 testing.

Deterministic from a fixed seed so the same pubkey/seckey pair gets
regenerated across runs. Production must use a random seed.
"""
from Crypto.PublicKey import ECC

seed = b'OpenLoad-demo-ed25519-seed-v1!!!'  # 32 bytes
assert len(seed) == 32

key = ECC.construct(curve='Ed25519', seed=seed)
pubkey = key.public_key().export_key(format='raw')
print('seed    :', seed.hex(), '(32B)')
print('pubkey  :', pubkey.hex(), '(32B)')
print()
print('// OPENLOAD_ED25519_PUBKEY_BYTES initializer list:')
for i in range(0, 32, 8):
    line = ','.join(f'0x{b:02X}' for b in pubkey[i:i+8])
    suffix = ', \\' if i < 24 else ''
    print('    ' + line + suffix)
