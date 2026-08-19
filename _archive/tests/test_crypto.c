/* test_crypto.c — valida o port em C da crypto XOR do Cabal (CABALREVERSE).
 *
 * Porta exata de PacketManager.cpp/PacketManager.h (x86 EP33, cliente Cabal):
 *   - LCG  CKeyRand::Rand (32-bit modular, shift 0x10)
 *   - keychain 1a metade: seed RECV_XORSEED (0x8F54C37B|1), 16384 dwords
 *   - DecodePacket: cada dword cru ^ dwXorKey; dwXorKey = keytable[raw & 0x3FFF];
 *     fechamento do tail com dwMask[len&3].
 *
 * Retorna 0 se todas as checagens passarem. Imprime a keychain p/ comparar com
 * um referencial independente (test_crypto_ref.py).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RW_RECV_KEY  0x7AB38CF1
#define RW_SEED      (0x8F54C37B | 1)
#define RW_KEYNUM    16384
#define RW_MAGIC     0xB7E2   /* 0xB7D9 + _PROTODEF_VERSION_(9) */
#define RW_MAGIC_EXT 0xC8F3

static unsigned int g_holdrand;

static void ck_seed(unsigned int s) { g_holdrand = s; }

/* CKeyRand::Rand — traducao literal (aritmetica 32-bit modular). */
static int ck_rand(void) {
    g_holdrand = g_holdrand * 0x2F6B6F5u + 0x14698B7u;
    return (int)((((g_holdrand >> 0x10) * 0x27F41C3u + 0xB327BDu) >> 0x10));
}

/* gera a 1a metade da keychain (igual a CXorKeyTable). */
static void gen_keytable(unsigned int* kt) {
    ck_seed(RW_SEED);
    for (int i = 0; i < RW_KEYNUM; ++i) {
        unsigned int wLow  = (unsigned short)ck_rand();
        unsigned int wHigh = (unsigned short)ck_rand();
        kt[i] = (wLow & 0xFFFF) | ((wHigh & 0xFFFF) << 16);
    }
}

static const unsigned int dwMaskArr[4] = { 0xFFFFFFFF, 0xFFFFFF00, 0xFFFF0000, 0xFF000000 };

/* DECRIPTA (RX; = DecodePacket): avanca o chain pelo low14 do dword cru.
 * O dword atual eh lido ANTES do xor; o proximo dwXorKey usa esse valor cru. */
static void crypt_decrypt(unsigned char* buf, int len, const unsigned int* kt) {
    unsigned int dwXorKey = RW_RECV_KEY;
    int i;
    for (i = 0; i < len / 4; ++i) {
        unsigned int* pw = (unsigned int*)(buf + i * 4);
        unsigned int raw = *pw;
        unsigned int old = raw & 0x00003FFF;
        *pw = raw ^ dwXorKey;
        dwXorKey = kt[old];
    }
    int tail = len & 3;
    if (tail) {
        unsigned char* tp = buf + (len / 4) * 4;
        unsigned int result = dwXorKey & ~dwMaskArr[tail];
        for (int k = 0; k < tail; ++k)
            tp[k] ^= (unsigned char)(result >> (8 * k));
    }
}

/* ENCRIPTA (o lado do "servidor"/wire): avanca o chain pelo low14 do
 * ciphertext que ACABA DE PRODUZIR (o que o DecodePacket do cliente vera). */
static void crypt_encrypt(unsigned char* buf, int len, const unsigned int* kt) {
    unsigned int dwXorKey = RW_RECV_KEY;
    int i;
    for (i = 0; i < len / 4; ++i) {
        unsigned int* pw = (unsigned int*)(buf + i * 4);
        unsigned int out = (*pw) ^ dwXorKey;
        *pw = out;
        dwXorKey = kt[out & 0x00003FFF];
    }
    int tail = len & 3;
    if (tail) {
        unsigned char* tp = buf + (len / 4) * 4;
        unsigned int result = dwXorKey & ~dwMaskArr[tail];
        for (int k = 0; k < tail; ++k)
            tp[k] ^= (unsigned char)(result >> (8 * k));
    }
}

/* monta uma faixa de alvos e valida: enc = encrypt(plain); decrypt(enc) == plain. */
static int test_roundtrip(const unsigned int* kt) {
    static unsigned char seed = 0x5A;
    int fails = 0;
    int lens[] = { 4, 6, 7, 8, 10, 12, 20, 33, 64, 300, 0 };
    for (int li = 0; lens[li] != 0; ++li) {
        int n = lens[li];
        unsigned char* a = (unsigned char*)calloc(n + 8, 1);
        unsigned char* b = (unsigned char*)calloc(n + 8, 1);
        for (int i = 0; i < n; ++i) { seed = (unsigned char)(seed * 7 + 13); a[i] = seed; }
        memcpy(b, a, n);
        crypt_encrypt(a, n, kt);   /* plain -> ciphertext (wire) */
        crypt_decrypt(a, n, kt);   /* ciphertext -> plain */
        if (memcmp(a, b, n) != 0) {
            printf("  FALHA round-trip len=%d\n", n);
            fails++;
        }
        free(a); free(b);
    }
    return fails;
}

/* monta um pacote S2C falso, encripta (wire) e valida o parse de header. */
static int test_header_parse(const unsigned int* kt) {
    /* payload de exemplo: uma mensagem de sistema */
    static const unsigned char payload[] = "WRONG PASS" " payload........";
    int paylen = (int)sizeof(payload);
    int total = 6 + paylen;              /* header sS2C_HEADER de 6 bytes */
    int alloc = (total + 3) & ~3;
    unsigned char* pkt = (unsigned char*)calloc(alloc, 1);

    /* escreve o plaintext do header: magic 0xB7E2 | (total<<16), maincmd 0x78 */
    unsigned int h0 = RW_MAGIC | ((unsigned int)total << 16);
    memcpy(pkt, &h0, 4);
    unsigned short maincmd = 0x78;      /* NFY_SYSTEMMESSG */
    memcpy(pkt + 4, &maincmd, 2);
    memcpy(pkt + 6, payload, paylen);

    /* encripta (wire) e depois decripta como o cliente faria */
    unsigned char* wire = (unsigned char*)calloc(alloc, 1);
    memcpy(wire, pkt, alloc);
    crypt_encrypt(wire, total, kt);

    unsigned char* dec = (unsigned char*)calloc(alloc, 1);
    memcpy(dec, wire, alloc);
    crypt_decrypt(dec, total, kt);

    unsigned int dec0;
    memcpy(&dec0, dec, 4);
    unsigned int magic = dec0 & 0xFFFF;
    unsigned int got_len = dec0 >> 16;
    unsigned short got_cmd;
    memcpy(&got_cmd, dec + 4, 2);

    int ok = (magic == RW_MAGIC) && (got_len == (unsigned int)total) && (got_cmd == 0x78);
    printf("  header parse: magic=%04X len=%u cmd=%04X %s\n",
           (unsigned)magic, got_len, (unsigned)got_cmd, ok ? "OK" : "FALHA");
    if (!ok) { printf("  dec0=%08X wire0=%08X\n", (unsigned)dec0, *(unsigned int*)wire); }
    free(wire); free(dec); free(pkt);
    return ok ? 0 : 1;
}

int main(void) {
    unsigned int* kt = (unsigned int*)malloc(sizeof(unsigned int) * RW_KEYNUM);
    gen_keytable(kt);

    printf("keytable[0..5]:\n");
    for (int i = 0; i < 6; ++i) printf("  [%d] = %08X\n", i, kt[i]);

    int fails = 0;
    fails += test_roundtrip(kt);
    fails += test_header_parse(kt);

    printf(fails ? "RESULTADO: %d FALHAS\n" : "RESULTADO: TUDO OK\n", fails);
    free(kt);
    return fails ? 1 : 0;
}