/* login_dll.c — CABAL_Login: DLL injetada que processa UMA conta por vez
 * (fluxo SEQUENCIAL, nao multi-instancia).
 *
 * Fluxo: o runner (login_sequential.py) abre o cliente, grava o indice da conta
 * em `login.current` e injeta. A DLL espera o cliente chegar na tela de login,
 * loga a conta (usuario/senha/subsenha), seleciona a char, VERIFICA (le o nivel/
 * ouro da memoria do proprio processo — offsets a localizar) e grava em
 * `output.txt`. O runner mata o cliente e parte pra proxima conta. */

#include "login.h"
#include "recvwatch.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static char g_dir[MAX_PATH];

/* Bypass anti-hack (MESMO padrao do proxy): jne rel32 -> jmp (mesmo alvo).
 * Soh patcha DEPOIS do .text decifrar (a thread dorme antes, como o proxy).
 * Por padrao patcha SO 0x352CDB (o mesmo que o proxy usa e que funciona).
 * 0x34D1AD (indicado pelo usuario) e OPCIONAL via login.cfg: "antihack2 1" —
 * nunca confirmado ao vivo; deixar desligado evita corromper outra funcao.
 * Tudo protegido por SEH — um AV nunca derruba o jogo. */
static int g_antihack2 = 0;   /* 0x34D1AD ligado? */

static void patch_one(uintptr_t base, DWORD rva) {
    BYTE* p = (BYTE*)(base + rva);
    __try {
        if (*(WORD*)p != 0x850F) return;             /* espera 0F 85 (jne rel32) */
        DWORD old;
        if (!VirtualProtect(p, 6, PAGE_EXECUTE_READWRITE, &old)) return;
        DWORD rel32 = *(DWORD*)(p + 2);
        p[0] = 0xE9;                                 /* jmp rel32 */
        *(DWORD*)(p + 1) = rel32 + 1;                /* mesmo alvo (jne 6B -> jmp 5B) */
        p[5] = 0x90;
        VirtualProtect(p, 6, old, &old);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        /* nao deixa um AV no patch derrubar o jogo */
    }
}

static void patch_antihack(void) {
    uintptr_t base = (uintptr_t)GetModuleHandleA("CabalMain.exe");
    if (!base) return;
    patch_one(base, 0x352CDB);   /* bypass principal (o mesmo que o proxy usa) */
    if (g_antihack2)
        patch_one(base, 0x34D1AD);   /* opcional — desligado por padrao */
}

static void get_dll_dir(void) {
    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&get_dll_dir, &self);
    char p[MAX_PATH] = { 0 };
    if (self) GetModuleFileNameA(self, p, MAX_PATH);
    char* slash = strrchr(p, '\\');
    if (slash) *slash = 0;
    if (!p[0]) strcpy_s(p, MAX_PATH, "D:\\projeto\\CABAL_Login");
    /* sobe ate achar a RAIZ do projeto (pasta que contem 'Cabal BR SUB.txt') */
    for (int up = 0; up < 4; up++) {
        char test[MAX_PATH];
        sprintf_s(test, MAX_PATH, "%s\\Cabal BR SUB.txt", p);
        FILE* f = NULL;
        fopen_s(&f, test, "r");
        if (f) { fclose(f); break; }
        slash = strrchr(p, '\\');
        if (!slash) break;
        *slash = 0;
    }
    strncpy_s(g_dir, MAX_PATH, p[0] ? p : "D:\\projeto\\CABAL_Login", _TRUNCATE);
}

static int read_current_index(void) {
    char path[MAX_PATH];
    sprintf_s(path, MAX_PATH, "%s\\login.current", g_dir);
    FILE* f = NULL;
    fopen_s(&f, path, "r");
    if (!f) return 0;
    int idx = 0;
    fscanf_s(f, "%d", &idx);
    fclose(f);
    return idx;
}

/* login.cfg: "entrar <x> <y>" = posicao do botao Entrar; "antihack2 1" liga o
 * patch 0x34D1AD (opcional, desligado por padrao). */
static void read_entrar_pos(void) {
    char path[MAX_PATH];
    sprintf_s(path, MAX_PATH, "%s\\login.cfg", g_dir);
    FILE* f = NULL;
    fopen_s(&f, path, "r");
    if (!f) return;
    char key[32]; int a = -1, b = -1;
    while (fscanf_s(f, "%31s %d %d", key, (unsigned)sizeof(key), &a, &b) >= 2) {
        if (strnicmp(key, "entrar", 6) == 0) login_set_entrar(a, b);
        else if (strnicmp(key, "antihack2", 9) == 0) g_antihack2 = (a != 0);
    }
    fclose(f);
}

/* VERIFICACAO: login_verify_enriched (em login.c) le ouro/nivel/nome +
     * classe/nacao/rank/mapa/guild do USERDATACONTEXT (se o build casar os
     * offsets) ou do legado base+0x137F1xx. Sempre grava em `out` (append). */

/* Gatilho MANUAL: F8 = iniciar login; F9 = modo de mapeamento dos botoes
 * (passa o mouse sobre cada botao e aperta F9 de novo). */
static void login_wait_trigger(FILE* log) {
    login_log(log, "[login] PRONTO — F8=login | F9=mapear botoes...\n");
    for (;;) {
        /* Usa o estado fisico (0x8000), nao o bit de transicao (bit 0):
         * o jogo/DirectInput pode consumir o bit antes desta thread. */
        if (GetAsyncKeyState(VK_F8) & 0x8000) {
            while (GetAsyncKeyState(VK_F8) & 0x8000) Sleep(50); /* debounce */
            break;
        }
        if (GetAsyncKeyState(VK_F9) & 0x8000) {              /* mapeia botoes */
            while (GetAsyncKeyState(VK_F9) & 0x8000) Sleep(50);
            login_map_buttons();
            login_log(log, "[login] PRONTO — F8=login | F9=mapear botoes...\n");
        }
        char go[MAX_PATH];
        sprintf_s(go, MAX_PATH, "%s\\login.go", g_dir);
        FILE* f = NULL;
        fopen_s(&f, go, "r");
        if (f) { fclose(f); remove(go); break; }
        Sleep(100);
    }
    login_log(log, "[login] gatilho acionado — iniciando login\n");
}

static DWORD WINAPI login_thread(LPVOID lp) {
    (void)lp;
    __try {
    /* igual o proxy: primeiro aguarda o jogo estabilizar (.text Themida decifra
     * em ~4s; o anti-cheat inicializa). Patchnar antes disso pode crashar o jogo. */
    Sleep(4000);
    get_dll_dir();
    patch_antihack();   /* agora o .text ja decifrou */
    read_entrar_pos();  /* antihack2 (e entrar antigo) */
    login_read_cfg();   /* posicoes dos 7 botoes p/ o fluxo completo */

    char logPath[MAX_PATH];
    sprintf_s(logPath, MAX_PATH, "%s\\login_%d.log", g_dir, read_current_index());
    FILE* log = NULL;
    fopen_s(&log, logPath, "w");
    if (log) {
        setvbuf(log, NULL, _IONBF, 0);   /* sem buffer: cada passo aparece na hora */
        login_log(log, "# CABAL_Login — DLL carregada\n");
    }

    /* CONSOLE em tempo real */
    if (AllocConsole()) {
        FILE* con = NULL;
        freopen_s(&con, "CONOUT$", "w", stdout);
        login_set_console(stdout);
        SetConsoleTitleA("CABAL_Login — logs em tempo real");
        login_log(log, "[console] ativado — F8=login | F9=mapear botoes\n");
    }

    login_log(log, "[login] preparando gatilho F8\n");
    /* F8 somente inicia o login. O recvwatch e a classificação de rede não
     * fazem parte do fluxo direto e não são inicializados. */
    login_wait_trigger(log);
    login_log(log, "[login] F8 detectado; iniciando login\n");

    char brPath[MAX_PATH];
    sprintf_s(brPath, MAX_PATH, "%s\\Cabal BR SUB.txt", g_dir);
    LoginAccount accs[256];
    int n = login_parse_br(brPath, accs, 256);
    int idx = read_current_index();
    if (idx < 0 || idx >= n) idx = 0;
    if (n == 0) { login_log(log, "[login] ERRO: sem contas no arquivo\n"); return 0; }

    /* quantas contas processar a partir de `idx`:
     * - se existir login.n, processa N contas (a partir de idx);
     * - sem login.n, processa ATE O FIM (todas as restantes). */
    int limit = n;
    {
        FILE* f = NULL;
        char np[MAX_PATH];
        sprintf_s(np, MAX_PATH, "%s\\login.n", g_dir);
        fopen_s(&f, np, "r");
        if (f) {
            int v = 0;
            fscanf_s(f, "%d", &v);
            fclose(f);
            if (v > 0 && idx + v < limit) limit = idx + v;
        }
    }
    login_log(log, "[login] loop de %d conta(s): %d..%d\n", limit - idx, idx, limit - 1);

    char outPath[MAX_PATH];
    sprintf_s(outPath, MAX_PATH, "%s\\output.txt", g_dir);

    for (int acc = idx; acc < limit; ++acc) {
        login_log(log, "# conta %d/%d (%s)\n", acc, limit, accs[acc].user);

        /* para forcar parada: criar login.stop na raiz do projeto */
        {
            char sp[MAX_PATH];
            sprintf_s(sp, MAX_PATH, "%s\\login.stop", g_dir);
            FILE* f = NULL;
            fopen_s(&f, sp, "r");
            if (f) { fclose(f); login_log(log, "[login] login.stop presente — parando\n"); break; }
        }

        recvwatch_reset();                    /* observacao limpa p/ esta conta */
        if (!login_run_account(&accs[acc], log)) {
            login_log(log, "[login] conta %d nao concluida; output.txt nao sera atualizado\n", acc);
            break;
        }

        /* espera entrar no mundo e le a estrutura do personagem */
        Sleep(4000);
        {
            void* ud = login_find_userdata();
            if (ud) login_log(log, "[login] USERDATACONTEXT encontrado (src enriquecida)\n");
            else    login_log(log, "[login] userdata nao encontrado — legado base+0x137F1xx\n");
        }

        FILE* out = NULL;
        fopen_s(&out, outPath, "a");          /* append: soma de todas as contas */
        if (out) {
            login_verify_enriched(&accs[acc], out);
            fclose(out);
        }
        login_log(log, "[login] conta %d finalizada (verificacao em output.txt)\n", acc);

        /* entre contas: confirma a volta a tela de login + respiro */
        login_wait_login_screen(15000);
        Sleep(1500);

        /* sinaliza que esta conta foi processada */
        char donePath[MAX_PATH];
        sprintf_s(donePath, MAX_PATH, "%s\\done_%d", g_dir, acc);
        FILE* done = NULL;
        fopen_s(&done, donePath, "w");
        if (done) fclose(done);
    }

    login_log(log, "[login] sequencia S2C do ultimo login (debug):\n");
    recvwatch_dump(log);
    login_log(log, "[login] done — loop de %d conta(s) concluido\n", limit - idx);
    if (log) fclose(log);

    /* sinaliza o fim do loop (o client nao e fechado — fica na tela de login) */
    char doneAllPath[MAX_PATH];
    sprintf_s(doneAllPath, MAX_PATH, "%s\\done_all", g_dir);
    FILE* doneAll = NULL;
    fopen_s(&doneAll, doneAllPath, "w");
    if (doneAll) fclose(doneAll);
    return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        /* qualquer falha do driver nunca derruba o jogo — so sai da thread */
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)hModule; (void)lpReserved;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        /* NADA pesado aqui (nem patch) — so cria a thread, como o proxy. */
        HANDLE t = CreateThread(NULL, 0, login_thread, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
