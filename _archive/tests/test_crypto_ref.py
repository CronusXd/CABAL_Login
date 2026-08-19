#!/usr/bin/env python3
"""test_crypto_ref.py — referencial independente da crypto XOR do Cabal.

Mesma fonte (CABALREVERSE PacketManager.cpp): LCG CKeyRand::Rand, keychain
1a metade (seed 0x8F54C37B|1, 16384 dwords), DecodePacket com SEND_XORKEY
0x7AB38CF1 e chain por raw&0x3FFF + fechamento de tail.

Uso: python test_crypto_ref.py  -> imprime keytable[0..5] e resultado de um
round-trip. Comparar a saida com a de test_crypto.exe (dentro (<-MASK 32)).
"""
RECV_XORKEY = 0x7AB38CF1
RECV_XORSEED = 0x8F54C37B | 1
KEYNUM = 16384
MASK32 = (1 << 32) - 1


def ck_seed(s):
    global holdrand
    holdrand = s & MASK32


def ck_rand():
    global holdrand
    holdrand = (holdrand * 0x2F6B6F5 + 0x14698B7) & MASK32
    x = ((holdrand >> 0x10) * 0x27F41C3 + 0xB327BD) & MASK32
    return x >> 0x10


def gen_keytable():
    ck_seed(RECV_XORSEED)
    kt = []
    for _ in range(KEYNUM):
        wlow = ck_rand() & 0xFFFF
        whigh = ck_rand() & 0xFFFF
        kt.append((wlow & 0xFFFF) | ((whigh & 0xFFFF) << 16))
    return kt


def crypt_decrypt(buf: bytearray, kt):
    """RX (DecodePacket): avanca o chain pelo low14 do dword cru (ciphertext)."""
    dwxorkey = RECV_XORKEY
    n = len(buf)
    for i in range(n // 4):
        raw = int.from_bytes(buf[i*4:i*4+4], "little")
        old = raw & 0x3FFF
        out = raw ^ dwxorkey
        buf[i*4:i*4+4] = out.to_bytes(4, "little")
        dwxorkey = kt[old]
    tail = n & 3
    if tail:
        result = dwxorkey & ~(0xFFFFFFFF << (8 * (4 - tail))) & 0xFFFFFFFF
        base = (n // 4) * 4
        for k in range(tail):
            buf[base + k] ^= (result >> (8 * k)) & 0xFF
    return buf


def crypt_encrypt(buf: bytearray, kt):
    """wire: avanca o chain pelo low14 do ciphertext recem-produzido."""
    dwxorkey = RECV_XORKEY
    n = len(buf)
    for i in range(n // 4):
        out = int.from_bytes(buf[i*4:i*4+4], "little") ^ dwxorkey
        buf[i*4:i*4+4] = out.to_bytes(4, "little")
        dwxorkey = kt[out & 0x3FFF]
    tail = n & 3
    if tail:
        result = dwxorkey & ~(0xFFFFFFFF << (8 * (4 - tail))) & 0xFFFFFFFF
        base = (n // 4) * 4
        for k in range(tail):
            buf[base + k] ^= (result >> (8 * k)) & 0xFF
    return buf


def test_roundtrip(kt):
    fails = 0
    for n in (4, 6, 7, 8, 10, 12, 20, 33, 64, 300):
        orig = bytes((i * 37 + 11) & 0xFF for i in range(n))
        a = bytearray(orig)
        crypt_encrypt(a, kt)
        crypt_decrypt(a, kt)
        if bytes(a) != orig:
            print(f"  FALHA round-trip len={n}")
            fails += 1
    return fails


def test_header(kt):
    payload = b"WRONG PASS" + b" payload........"
    total = 6 + len(payload)
    pkt = bytearray(total)
    h0 = 0xB7E2 | (total << 16)
    pkt[0:4] = h0.to_bytes(4, "little")
    pkt[4:6] = (0x78).to_bytes(2, "little")   # NFY_SYSTEMMESSG
    pkt[6:] = payload
    wire = bytearray(pkt)
    crypt_encrypt(wire, kt)
    dec = bytearray(wire)
    crypt_decrypt(dec, kt)
    dec0 = int.from_bytes(dec[0:4], "little")
    magic = dec0 & 0xFFFF
    got_len = dec0 >> 16
    got_cmd = int.from_bytes(dec[4:6], "little")
    ok = magic == 0xB7E2 and got_len == total and got_cmd == 0x78
    print(f"  header parse: magic={magic:04X} len={got_len} cmd={got_cmd:04X} "
          + ("OK" if ok else "FALHA"))
    return 0 if ok else 1


holdrand = 0
if __name__ == "__main__":
    kt = gen_keytable()
    print("keytable[0..5]:")
    for i in range(6):
        print(f"  [{i}] = {kt[i]:08X}")
    f = 0
    f += test_roundtrip(kt)
    f += test_header(kt)
    print("RESULTADO: TUDO OK" if f == 0 else f"RESULTADO: {f} FALHAS")
    raise SystemExit(1 if f else 0)