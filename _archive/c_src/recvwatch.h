#pragma once
/* recvwatch.h — observa o RECV do cliente Cabal (ws2_32) e decripta a
 * keychain XOR (porta do CABALREVERSE) para classificar a falha de login.
 *
 * Nao toca nos dados do jogo: so inicia o hook, faz reassembly por socket e
 * guarda o que foi observado. Tudo protegido; sem hook = nao muda nada.
 */

#include <windows.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* instala os hooks no IAT do processo (recv e, se existir, WSARecv).
 * idempotente. Devuelve 1 se pelo menos um hook entrou. */
int  recvwatch_init(void);

/* restaura os thunks do IAT (nao chamado em shutdown normal). */
void recvwatch_shutdown(void);

typedef enum {
    RW_CLASS_NONE = 0,   /* nada de falha observado ainda */
    RW_CLASS_WRONGPASS,  /* senha/sessao invalida */
    RW_CLASS_BLOCKED,    /* conta bloqueada/suspensa */
    RW_CLASS_FULL,       /* servidor/canal cheio */
    RW_CLASS_MAINT,      /* manutencao/estado do servidor */
    RW_CLASS_OTHER,      /* mensagem de sistema sem classificacao clara */
} RecvWatchClass;

/* limpa o estado observado (chamar antes de cada tentativa de login). */
void recvwatch_reset(void);

/* classifica a ultima troca a partir do que foi visto. */
RecvWatchClass recvwatch_classify(void);

/* copia o texto da ultima mensagem de sistema (ASCII best-effort). */
void recvwatch_last_msg(char* out, int max);

/* sequencia de maincmds decodificados (debug/analise). */
int  recvwatch_cmdcount(void);
int  recvwatch_cmd_at(int i);      /* maincmd */
int  recvwatch_cmdlen_at(int i);   /* comprimento decode */

/* imprime a sequencia (maincmd XXXX len N ...) em f. */
void recvwatch_dump(FILE* f);

#ifdef __cplusplus
}
#endif