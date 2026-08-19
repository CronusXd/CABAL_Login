/* test_recvwatch.c — exercita o pipeline do recvwatch sem o jogo:
 * gera pacotes S2C sinteticos (keychain do CABALREVERSE), encripta como o
 * wire, alimenta observe() (reassembly + decrypt) e valida a classificacao
 * de falha e a decodificacao do maincmd.
 *
 * Compila recvwatch.c com `static` desfeito p/ acessar as funcoes internas.
 */

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* headers normais PRIMEIRO; depois desfaz `static` e puxa a implementacao */
#define static
#include "../src/recvwatch.c"
#undef static

/* encripta um bloco (como o "servidor" faria) usando a keytable do recvwatch */
static void enc(unsigned char* buf, int len) {
    unsigned int dwXorKey = RW_RECV_KEY;
    for (int i = 0; i < len / 4; ++i) {
        unsigned int* pw = (unsigned int*)(buf + i * 4);
        unsigned int out = (*pw) ^ dwXorKey;
        *pw = out;
        dwXorKey = g_keytable[out & 0x3FFF];
    }
    int tail = len & 3;
    if (tail) {
        static const unsigned int mask[4] = { 0xFFFFFFFF, 0xFFFFFF00, 0xFFFF0000, 0xFF000000 };
        unsigned char* tp = buf + (len / 4) * 4;
        unsigned int result = dwXorKey & ~mask[tail];
        for (int k = 0; k < tail; ++k) tp[k] ^= (unsigned char)(result >> (8 * k));
    }
}

static int g_fails = 0;
#define CHECK(cond, msg) do { if (cond) printf("  ok   %s\n", msg); \
    else { printf("  FALHA %s\n", msg); g_fails++; } } while (0)

int main(void) {
    recvwatch_init();   /* inicializa a critical section (hooks nao acham CabalMain: ok) */
    ensure_keys();

    /* pacote S2C falso: header magic 0xB7E2 | len<<16, maincmd 0x78 (NFY_SYSTEMMESSG),
     * payload com "WRONG PASSWORD" */
    char pub[] = "WRONG PASSWORD. Tente novamente.";
    int total = 6 + (int)sizeof(pub);
    unsigned char pkt[256];
    memset(pkt, 0, sizeof pkt);
    unsigned int h0 = RW_MAGIC | ((unsigned int)total << 16);
    memcpy(pkt, &h0, 4);
    unsigned short cmd = 0x78;
    memcpy(pkt + 4, &cmd, 2);
    memcpy(pkt + 6, pub, sizeof pub);

    /* 1) entrega em UM bloco -> classifica WRONGPASS */
    {
        unsigned char wire[256];
        memcpy(wire, pkt, total);
        enc(wire, total);
        recvwatch_reset();
        observe((SOCKET)0x100, wire, total);
        CHECK(recvwatch_cmdcount() == 1, "1 pacote parseado (1 bloco)");
        CHECK(recvwatch_cmd_at(0) == 0x78, "maincmd = 0x78");
        CHECK(recvwatch_classify() == RW_CLASS_WRONGPASS, "classificou senha_invalida");
        char m[160]; recvwatch_last_msg(m, sizeof m);
        CHECK(strstr(m, "WRONG PASSWORD") != NULL, "mensagem extraída");
        printf("      msg='%s'\n", m);
    }

    /* 2) pacote parcelado em 2 recv -> reassembly funciona */
    {
        unsigned char wire[256];
        memcpy(wire, pkt, total);
        enc(wire, total);
        recvwatch_reset();
        observe((SOCKET)0x101, wire, 9);
        observe((SOCKET)0x101, wire + 9, total - 9);
        CHECK(recvwatch_cmdcount() == 1, "1 pacote parseado (parcelado 9+n)");
        CHECK(recvwatch_classify() == RW_CLASS_WRONGPASS, "classificou (parcelado)");
    }

    /* 3) dois pacotes no mesmo recv */
    {
        unsigned char wire[512];
        int t2 = total;
        memcpy(wire, pkt, t2);
        enc(wire, t2);                              /* pacote 1 */
        unsigned char p2[128];
        memset(p2, 0, sizeof p2);
        unsigned int h02 = RW_MAGIC | ((unsigned int)(6 + 12) << 16);
        memcpy(p2, &h02, 4);
        unsigned short cmd2 = 0x65;                 /* Connect2Svr S2C */
        memcpy(p2 + 4, &cmd2, 2);
        unsigned char body2[12]; memset(body2, 0xAB, sizeof body2);
        memcpy(p2 + 6, body2, sizeof body2);
        memcpy(wire + t2, p2, 18);
        enc(wire + t2, 18);
        recvwatch_reset();
        observe((SOCKET)0x102, wire, t2 + 18);
        CHECK(recvwatch_cmdcount() == 2, "2 pacotes no mesmo recv");
        CHECK(recvwatch_cmd_at(0) == 0x78 && recvwatch_cmd_at(1) == 0x65,
              "maincmds 0x78 e 0x65 na ordem");
    }

    /* 4) reset limpa o estado */
    {
        recvwatch_reset();
        CHECK(recvwatch_cmdcount() == 0, "reset zera comandos");
        CHECK(recvwatch_classify() == RW_CLASS_NONE, "reset zera classificacao");
    }

    /* 5) buffer com byte de lixo na frente (stream mal começada) */
    {
        unsigned char wire[256];
        memcpy(wire + 1, pkt, total);   /* 1 byte de lixo + pacote */
        enc(wire + 1, total);
        wire[0] = 0x42;
        recvwatch_reset();
        observe((SOCKET)0x103, wire, total + 1);
        CHECK(recvwatch_cmdcount() == 1, "lixo 1 byte descartado, pacote ok");
        CHECK(recvwatch_classify() == RW_CLASS_WRONGPASS, "classificou (com lixo)");
    }

    printf(g_fails ? "RESULTADO: %d FALHAS\n" : "RESULTADO: TUDO OK\n", g_fails);
    return g_fails ? 1 : 0;
}