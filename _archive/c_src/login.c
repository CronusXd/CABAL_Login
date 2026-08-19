/* login.c — CABAL_Login: driver de login (ler contas + SendInput). */

#include "login.h"
#include "recvwatch.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

static char g_mapbuf[512] = { 0 };   /* acumula os botoes mapeados p/ login.cfg */

/* posicoes dos botoes (pixels) lidas do login.cfg — -1 = nao configurado */
static int g_pos_entrar_x = -1, g_pos_entrar_y = -1;
static int g_pos_servidor_x = -1, g_pos_servidor_y = -1;   /* Mercurio/Venus no seletor */
static int g_pos_canal_x = -1, g_pos_canal_y = -1;         /* Canal especifico (ex: 9) */
static int g_pos_comeca_x = -1, g_pos_comeca_y = -1;       /* Iniciar na char */
static int g_pos_selserv_x = -1, g_pos_selserv_y = -1;     /* menu -> selecionar servidor */
static int g_pos_sim_x = -1, g_pos_sim_y = -1;
static int g_pos_desc_x = -1, g_pos_desc_y = -1;           /* desconectar */

/* Forward declarations p/ uso antecipado */
static FILE* g_console = NULL;
static void lf(FILE* f, const char* fmt, ...);

/* Le login.cfg (D:\projeto\CABAL_Login\login.cfg) e carrega todas as posicoes. */
void login_read_cfg(void) {
    FILE* f = NULL;
    fopen_s(&f, "D:\\projeto\\CABAL_Login\\login.cfg", "r");
    if (!f) return;
    char key[32]; int a = -1, b = -1;
    while (fscanf_s(f, "%31s %d %d", key, (unsigned)sizeof(key), &a, &b) >= 2) {
        if      (!strnicmp(key, "entrar", 6))              { g_pos_entrar_x = a; g_pos_entrar_y = b; }
        else if (!strnicmp(key, "servidor", 8))            { g_pos_servidor_x = a; g_pos_servidor_y = b; }
        else if (!strnicmp(key, "canal", 5))               { g_pos_canal_x = a; g_pos_canal_y = b; }
        else if (!strnicmp(key, "comeca", 6))              { g_pos_comeca_x = a; g_pos_comeca_y = b; }
        else if (!strnicmp(key, "selecionar_servidor", 19)){ g_pos_selserv_x = a; g_pos_selserv_y = b; }
        else if (!strnicmp(key, "sim", 3))                 { g_pos_sim_x = a; g_pos_sim_y = b; }
        else if (!strnicmp(key, "desconectar", 11))        { g_pos_desc_x = a; g_pos_desc_y = b; }
    }
    fclose(f);
}

int login_load_accounts(const char* path, LoginAccount* out, int max) {
    if (!path || !out || max <= 0) return 0;
    FILE* f = NULL;
    fopen_s(&f, path, "r");
    if (!f) return 0;
    int n = 0;
    char line[256];
    while (fgets(line, sizeof(line), f) && n < max) {
        /* tira \r\n */
        size_t L = strlen(line);
        while (L && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;
        if (!line[0] || line[0] == '#') continue;
        /* separa "user:pass:charname" */
        char* c1 = strchr(line, ':');
        if (!c1) continue;
        *c1 = 0;
        char* user = line;
        char* c2 = strchr(c1 + 1, ':');
        char* pass;
        char* cn = "";
        if (c2) {
            *c2 = 0;
            pass = c1 + 1;
            cn = c2 + 1;
        } else {
            pass = c1 + 1;
        }
        strncpy_s(out[n].user, 64, user, _TRUNCATE);
        strncpy_s(out[n].pass, 64, pass, _TRUNCATE);
        strncpy_s(out[n].charname, 64, cn, _TRUNCATE);
        n++;
    }
    fclose(f);
    return n;
}

int login_parse_br(const char* path, LoginAccount* out, int max) {
    if (!path || !out || max <= 0) return 0;
    FILE* f = NULL;
    fopen_s(&f, path, "r");
    if (!f) return 0;
    int n = 0;
    char line[256];
    LoginAccount cur; memset(&cur, 0, sizeof(cur));
    int have = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t L = strlen(line);
        while (L && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;
        if (!line[0]) continue;
        /* "Conta N" -> fecha a conta anterior */
        if (strncmp(line, "Conta", 5) == 0) {
            if (have && cur.user[0] && cur.pass[0] && n < max) {
                out[n++] = cur;
            }
            memset(&cur, 0, sizeof(cur));
            have = 1;
            continue;
        }
        /* "Usuario: x" / "Senha: x" / "subsenha: x" / "NV: x" */
        char* colon = strchr(line, ':');
        if (!colon) continue;
        *colon = 0;
        char* val = colon + 1;
        while (*val == ' ') val++;
        const char* key = line;
        if (strnicmp(key, "Usuario", 7) == 0)      strncpy_s(cur.user, 64, val, _TRUNCATE);
        else if (strnicmp(key, "Senha", 5) == 0)   strncpy_s(cur.pass, 64, val, _TRUNCATE);
        else if (strnicmp(key, "subsenha", 8) == 0)strncpy_s(cur.subsenha, 64, val, _TRUNCATE);
        else if (strnicmp(key, "NV", 2) == 0)      cur.nivel = atoi(val);
    }
    if (have && cur.user[0] && cur.pass[0] && n < max) out[n++] = cur;
    fclose(f);
    return n;
}

/* Envia um teclado VK (keydown+keyup). Usa SCANCODE p/ Enter/Tab — o DirectInput
 * do jogo le scancode, nao VK; Enter sem scancode as vezes nao registra. */
static void send_vk(WORD vk) {
    WORD scan = 0;
    if (vk == VK_RETURN) scan = 0x1C;   /* Enter */
    else if (vk == VK_TAB) scan = 0x0F; /* Tab */
    INPUT in[2] = { 0 };
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = vk;
    in[0].ki.wScan = scan;
    in[0].ki.dwFlags = scan ? KEYEVENTF_SCANCODE : 0;
    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = vk;
    in[1].ki.wScan = scan;
    in[1].ki.dwFlags = (scan ? KEYEVENTF_SCANCODE : 0) | KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

/* Ctrl+<key>: tecla com Ctrl pressionado (ex.: Ctrl+A = seleciona tudo). */
static void key_combo(WORD ctrl, WORD key) {
    INPUT down[2] = { 0 };
    down[0].type = INPUT_KEYBOARD; down[0].ki.wVk = ctrl;
    down[1].type = INPUT_KEYBOARD; down[1].ki.wVk = key;
    SendInput(2, down, sizeof(INPUT));
    Sleep(40);
    INPUT up[2] = { 0 };
    up[0].type = INPUT_KEYBOARD; up[0].ki.wVk = key; up[0].ki.dwFlags = KEYEVENTF_KEYUP;
    up[1].type = INPUT_KEYBOARD; up[1].ki.wVk = ctrl; up[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, up, sizeof(INPUT));
    Sleep(80);
}

void login_press_tab(void)   { send_vk(VK_TAB); }
void login_press_enter(void) { send_vk(VK_RETURN); }

void login_type_text(const char* s, int delay_ms) {
    if (!s) return;
    for (const char* p = s; *p; p++) {
        INPUT in[2] = { 0 };
        in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = 0;
        in[0].ki.wScan = (WORD)(unsigned char)*p; in[0].ki.dwFlags = KEYEVENTF_UNICODE;
        in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = 0;
        in[1].ki.wScan = (WORD)(unsigned char)*p; in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
        if (delay_ms > 0) Sleep((DWORD)delay_ms);
    }
}

void login_click(int x, int y) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (sw <= 0 || sh <= 0) return;
    INPUT in[3] = { 0 };
    in[0].type = INPUT_MOUSE;
    in[0].mi.dx = (int)(((long long)x * 65535) / sw);
    in[0].mi.dy = (int)(((long long)y * 65535) / sh);
    in[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    in[2].type = INPUT_MOUSE; in[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(3, in, sizeof(INPUT));
    Sleep(300);
}

/* posicao do botao Entrar (config) */
static int g_entrar_x = -1, g_entrar_y = -1;
void login_set_entrar(int x, int y) { g_entrar_x = x; g_entrar_y = y; }

/* Chama a funcao de toggle de UI do jogo (base+0x549FD8, objeto base+0x138ABF0). */
bool login_toggle_ui(int mode) {
    uintptr_t base = (uintptr_t)GetModuleHandleA("CabalMain.exe");
    if (!base) return false;
    void* ui = (void*)(base + 0x138ABF0);
    void (*toggle)(void*, int) = (void(*)(void*, int))(base + 0x549FD8);
    toggle(ui, mode);
    Sleep(500);
    return true;
}

/* Chama a funcao de DESCONECTAR do jogo (base+0x356B30): fecha as DUAS
 * conexoes (login + world) e volta pra tela de login — efeito de clicar em
 * "Desconectar" > "Sim" no menu O. this = buffer zerado; o jogo so usa
 * [this+0x458] p/ release opcional, e zerado => pula. SEH: nunca crasha. */
bool login_do_disconnect(void) {
    uintptr_t base = (uintptr_t)GetModuleHandleA("CabalMain.exe");
    if (!base) return false;
    static BYTE ctx[0x500];
    memset(ctx, 0, sizeof ctx);
    void (*disc)(void*) = (void(*)(void*))(base + 0x356B30);
    __try {
        disc(ctx);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    Sleep(800);
    return true;
}

/* Chama a funcao de SELECIONAR SERVIDOR do jogo (base+0x34F878): prepara o
 * estado do servidor e fecha SO o mundo (0x1413E6318), mantendo o login —
 * volta pra tela de selecao de servidor (efeito do menu O > Selecionar
 * Servidor > Sim). Sem argumentos. SEH p/ nunca crashar. */
bool login_do_server_select(void) {
    uintptr_t base = (uintptr_t)GetModuleHandleA("CabalMain.exe");
    if (!base) return false;
    void (*f)(void) = (void(*)(void))(base + 0x34F878);
    __try {
        f();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    Sleep(800);
    return true;
}

/* ── VALIDAÇÃO DE ESTADO (maquina de estados) ────────────────────────────
 * Regra: cada passo so avanca apos confirmar o anterior. Sinal primario:
 * socket da conexao de login ([conn1+0x08]): -1/0 = logado; outro = conectado.
 * Sempre tolerant a leitura (SEH) e com timeout. */

long long login_conn1_socket(void) {
    uintptr_t base = (uintptr_t)GetModuleHandleA("CabalMain.exe");
    if (!base) return 0;
    __try {
        void* conn = *(void**)(base + 0x13E6310);   /* conn1 (login) */
        if (!conn) return 0;
        return *(long long*)((BYTE*)conn + 0x08);   /* socket */
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool login_wait_logged_in(int timeout_ms) {
    DWORD t0 = GetTickCount();
    while ((int)(GetTickCount() - t0) < timeout_ms) {
        long long s = login_conn1_socket();
        if (s != 0 && s != -1) return true;         /* socket vivo */
        Sleep(300);
    }
    return false;
}

bool login_wait_login_screen(int timeout_ms) {
    DWORD t0 = GetTickCount();
    while ((int)(GetTickCount() - t0) < timeout_ms) {
        long long s = login_conn1_socket();
        if (s == 0 || s == -1) return true;         /* socket fechado = login */
        Sleep(300);
    }
    return false;
}

bool login_wait_char_ready(int timeout_ms) {
    uintptr_t base = (uintptr_t)GetModuleHandleA("CabalMain.exe");
    if (!base) return false;
    DWORD t0 = GetTickCount();
    while ((int)(GetTickCount() - t0) < timeout_ms) {
        __try {
            char name[24] = {0};
            memcpy(name, (void*)(base + 0x137F190), 20);
            if (name[0]) return true;               /* char listo */
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        Sleep(300);
    }
    return false;
}

bool login_wait_world(int timeout_ms) {
    uintptr_t base = (uintptr_t)GetModuleHandleA("CabalMain.exe");
    if (!base) return false;
    DWORD t0 = GetTickCount();
    while ((int)(GetTickCount() - t0) < timeout_ms) {
        __try {
            DWORD gold = *(DWORD*)(base + 0x137F170);
            BYTE n = *(BYTE*)(base + 0x137F190);
            if (gold != 0 || n != 0) return true;   /* dados do char carregados */
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        Sleep(300);
    }
    return false;
}

bool login_select_server(void) {
    /* Usa a posição mapeada do botão "servidor" (Mercúrio/Venus) no login.cfg.
     * O mapeamento é feito uma vez via F9 (login_map_buttons). */
    if (g_pos_servidor_x >= 0 && g_pos_servidor_y >= 0) {
        login_click(g_pos_servidor_x, g_pos_servidor_y);
        Sleep(2000);                          /* 2s entre ações */
        return true;
    }
    return false;
}

bool login_select_channel(void) {
    /* Na tela de canais: clica no canal mapeado (ex: canal 9) e depois Enter. */
    if (g_pos_canal_x >= 0 && g_pos_canal_y >= 0) {
        lf(g_console, "[login]   -> clicando CANAL em (%d,%d)\n", g_pos_canal_x, g_pos_canal_y);
        login_click(g_pos_canal_x, g_pos_canal_y);
    } else {
        lf(g_console, "[login]   AVISO: pos_canal nao configurado, pulando clique\n");
    }
    lf(g_console, "[login]   -> pressionando ENTER para confirmar canal\n");
    login_press_enter();
    Sleep(2000);                          /* 2s entre ações */
    return true;
}

bool login_start_character(void) {
    /* Usa a posição mapeada do botão "comeca" (Iniciar) no login.cfg.
     * O mapeamento é feito uma vez via F9 (login_map_buttons). */
    if (g_pos_comeca_x >= 0 && g_pos_comeca_y >= 0) {
        login_click(g_pos_comeca_x, g_pos_comeca_y);
        Sleep(2000);                          /* 2s entre ações */
        return true;
    }
    return false;
}

/* ── USERDATACONTEXT finder + verificação enriquecida ──────────────────────
 * O mapa dos offsets vem do CABALREVERSE (EP33). O finder resolve o ponteiro
 * do userdata em runtime (nao depende de endereco absoluto): se o build nao
 * casar os offsets, a validacao heuristica falha e caimos no legado. */

void* login_find_userdata(void);

static void* g_udc = NULL;          /* cache do finder */

int login_udc_int(void* ud, DWORD off) {
    if (!ud) return 0;
    int v = 0;
    __try { v = *(int*)((BYTE*)ud + off); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return v;
}

int login_udc_str(void* ud, DWORD off, DWORD lenoff, char* out, int max) {
    if (!ud || !out || max <= 0) return 0;
    out[0] = 0;
    int len = login_udc_int(ud, lenoff);
    if (len < 1 || len >= 0x400) return 0;   /* len suspeito -> pointer ou lixo */
    __try {
        BYTE* p = (BYTE*)ud + off;
        int n = len < (max - 1) ? len : (max - 1);
        char buf[64];
        int cp = n < 64 ? n : 63;
        memcpy(buf, p, cp);
        buf[cp] = 0;
        memcpy(out, buf, (size_t)n + 1);
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

/* Ranges de secoes executaveis do modulo (p/ varrer o .text limpo). */
typedef struct { uintptr_t st, en; } XRange;

static int module_exec_ranges(uintptr_t base, XRange* out, int max) {
    IMAGE_DOS_HEADER dos;
    __try {
        memcpy(&dos, (void*)base, sizeof dos);
        if (dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
        IMAGE_NT_HEADERS nt;
        memcpy(&nt, (void*)(base + (DWORD)dos.e_lfanew), sizeof nt);
        if (nt.Signature != IMAGE_NT_SIGNATURE) return 0;
        IMAGE_SECTION_HEADER sec[16];
        int ns = nt.FileHeader.NumberOfSections;
        if (ns > 16) ns = 16;
        memcpy(sec, (void*)(base + (DWORD)dos.e_lfanew + 24 + (DWORD)nt.FileHeader.SizeOfOptionalHeader),
               (size_t)ns * sizeof(IMAGE_SECTION_HEADER));
        int n = 0;
        for (int i = 0; i < ns && n < max; ++i) {
            if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                DWORD sz = sec[i].Misc.VirtualSize > sec[i].SizeOfRawData
                             ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
                out[n].st = base + sec[i].VirtualAddress;
                out[n].en = base + sec[i].VirtualAddress + sz;
                if (out[n].en > out[n].st && out[n].st < out[n].en) n++;
            }
        }
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static void* udc_base_mod(void) {
    return GetModuleHandleA("CabalMain.exe");
}

/* fim da imagem do modulo (SizeOfImage do PE) — p/ excluir enderecos da imagem. */
static uintptr_t module_image_end(uintptr_t base) {
    IMAGE_DOS_HEADER dos;
    __try {
        memcpy(&dos, (void*)base, sizeof dos);
        if (dos.e_magic != IMAGE_DOS_SIGNATURE) return base + 0x20000000;
        IMAGE_NT_HEADERS nt;
        memcpy(&nt, (void*)(base + (DWORD)dos.e_lfanew), sizeof nt);
        if (nt.Signature != IMAGE_NT_SIGNATURE) return base + 0x20000000;
        return base + nt.OptionalHeader.SizeOfImage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return base + 0x20000000;
    }
}

/* Pontuacao heuristica de um candidato a USERDATACONTEXT. Quanto maior, mais
 * parece. 0 = rejeitado. So exclui candidatos dentro de SECOES DE CODIGO —
 * validado ao vivo: o userdata pode viver DENTRO da imagem (secao de dados,
 * ex.: RVA 0xF71A98), entao "excluir a imagem toda" estaria errado. */
static int udc_score(void* p, uintptr_t base, const XRange* ex, int nex) {
    if (!p) return 0;
    uintptr_t pa = (uintptr_t)p;
    if (pa >= base && pa < base + 0x1000) return 0;          /* headers do PE */
    for (int i = 0; i < nex; ++i)                            /* secoes EXEC = nao dado */
        if (pa >= ex[i].st && pa < ex[i].en) return 0;
    int logon = 0, level = 0, namelen = 0;
    __try {
        logon   = *(int*)(pa + UDC_LOGGED);
        level   = *(int*)(pa + UDC_LEVEL);
        namelen = *(int*)(pa + UDC_NAMELEN);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    if (logon < 0 || logon > 1) return 0;
    if (level < 0 || level > 0x4000) return 0;
    if (namelen < 0 || namelen > 0x400) return 0;
    int score = (logon == 1 ? 4 : 0) + (level > 0 ? 2 : 0) + (namelen > 0 ? 2 : 0);
    if (namelen > 0 && namelen <= 32) {
        char nm[24] = { 0 };
        __try { memcpy(nm, (void*)(pa + UDC_NAME), 16); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        int pr = 0;
        for (int k = 0; k < 8 && k < namelen; ++k)
            if (nm[k] >= 32 && nm[k] < 127) pr++;
        if (pr >= 4) score += 3;
    }
    return score;
}

/* bonus se o nome do candidato casar com o do legado (confirmacao cruzada). */
static int udc_legacy_bonus(void* ud, uintptr_t base) {
    if (!ud || !base) return 0;
    char legacy[24] = { 0 };
    __try { memcpy(legacy, (void*)(base + 0x137F190), 20); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    if (!legacy[0]) return 0;
    char nm[20] = { 0 };
    if (login_udc_str(ud, UDC_NAME, UDC_NAMELEN, nm, sizeof nm) <= 0) return 0;
    return (_strnicmp(nm, legacy, 20) == 0) ? 10 : 0;
}

void* login_find_userdata(void) {
    if (g_udc) return g_udc;
    uintptr_t base = (uintptr_t)udc_base_mod();
    if (!base) return NULL;
    uintptr_t imgEnd = module_image_end(base);

    XRange ex[8];
    int nex = module_exec_ranges(base, ex, 8);

    void* best = NULL;
    int bestScore = 0;

    /* 1) hipotese fixa (conversao x86 0x00CECAC4 -> base+0x8ECAC4). VALIDADO ao
     *    vivo: nao vale neste build (lixo); mantida p/ builds que casem. */
    {
        void* p = NULL;
        __try { p = *(void**)(base + 0x8ECAC4); }
        __except (EXCEPTION_EXECUTE_HANDLER) { p = NULL; }
        if (p) {
            int s = udc_score(p, base, ex, nex) + udc_legacy_bonus(p, base);
            if (s > bestScore) { bestScore = s; best = p; }
        }
    }

    /* 2) scan das secoes EXEC por "48 8B XD ?? ?? ?? ??" (mov r64,[rip+disp32])
     *    para TODAS as 8 variantes de registrador. */
    static const unsigned char modrm[8] = { 0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D };
    for (int r = 0; r < nex; ++r) {
        uintptr_t a = ex[r].st, e = ex[r].en;
        for (uintptr_t p = a; p + 7 <= e; ++p) {
            if (*(unsigned char*)p != 0x48 || *(unsigned char*)(p + 1) != 0x8B) continue;
            unsigned char mr = *(unsigned char*)(p + 2);
            int okmod = 0;
            for (int k = 0; k < 8; ++k) if (mr == modrm[k]) { okmod = 1; break; }
            if (!okmod) continue;
            int s32 = 0;
            memcpy(&s32, (void*)(p + 3), 4);
            uintptr_t slot = p + 7 + (intptr_t)s32;
            if (slot + 8 < base || slot >= imgEnd) continue;   /* slot fora da imagem */
            void* cand = NULL;
            __try { cand = *(void**)slot; }
            __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
            int s = udc_score(cand, base, ex, nex) + udc_legacy_bonus(cand, base);
            if (s > bestScore) { bestScore = s; best = cand; }
        }
    }

    /* 2b) Themida/VM costuma carregar global como IMEDIATO: "48 B8 <imm64>".
     *     O imm64 ja eh o endereco do objeto (sem indirecao). */
    if (bestScore < 4) {
        for (int r = 0; r < nex; ++r) {
            uintptr_t a = ex[r].st, e = ex[r].en;
            for (uintptr_t p = a; p + 12 <= e; ++p) {
                if (*(unsigned char*)p != 0x48 || *(unsigned char*)(p + 1) != 0xB8) continue;
                uintptr_t imm64 = 0;
                memcpy(&imm64, (void*)(p + 2), 8);
                void* cand = (void*)imm64;
                int s = udc_score(cand, base, ex, nex) + udc_legacy_bonus(cand, base);
                if (s > bestScore) { bestScore = s; best = cand; }
            }
        }
    }

    /* aceita so candidatos com prova decente (nome legivel +nivel OU onLogged). */
    if (bestScore >= 4) g_udc = best;
    return g_udc;
}

/* Espera o onLogged do userdata virar 1 (sinal do jogo de "no mundo"). */
bool login_wait_onlogged(int timeout_ms) {
    void* ud = login_find_userdata();
    if (!ud) return false;
    DWORD t0 = GetTickCount();
    while ((int)(GetTickCount() - t0) < timeout_ms) {
        if (login_udc_int(ud, UDC_LOGGED) == 1) return true;
        Sleep(300);
    }
    return false;
}

/* ── Verificação enriquecida ─────────────────────────────────────────────── */
int login_verify_enriched(const LoginAccount* a, FILE* out) {
    char t[64] = "?";
    time_t now = time(NULL);
    struct tm tmv;
    if (localtime_s(&tmv, &now) == 0)
        strftime(t, sizeof t, "%Y-%m-%d %H:%M:%S", &tmv);

    uintptr_t base = (uintptr_t)udc_base_mod();
    if (!base) { login_log(out, "%s;user=%s;login=ok;erro=modulo_nao_encontrado\n", t, a->user); return 0; }

    void* ud = login_find_userdata();
    char src[16] = "legacy";

    /* legado (ouro e os campos ja confirmados desse build) */
    DWORD gold = 0; int lvlLegacy = 0; char nameLegacy[24] = { 0 };
    __try {
        gold = *(DWORD*)(base + 0x137F170);
        lvlLegacy = *(int*)(base + 0x137F1B8);
        memcpy(nameLegacy, (void*)(base + 0x137F190), 20);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    char name[32] = { 0 };
    int  lvl = 0;
    int  cls = 0, nat = 0, rank = 0, mapId = 0, logged = -1;
    char guild[32] = { 0 };

    if (ud) {
        if (login_udc_str(ud, UDC_NAME, UDC_NAMELEN, name, sizeof name) <= 0) {
            /* nome quebrado -> struct de outro build: cai pra legado */
            ud = NULL;
        } else {
            strcpy_s(src, sizeof src, "userdata");
        }
    }
    if (!ud) {
        strncpy_s(name, 32, nameLegacy[0] ? nameLegacy : "", _TRUNCATE);
        if (lvlLegacy > 0) lvl = lvlLegacy;
        else { /* tenta userdata mesmo assim p/ nivel (fallback parcial) */
            void* u2 = login_find_userdata();
            if (u2) lvl = login_udc_int(u2, UDC_LEVEL);
        }
    } else {
        lvl = login_udc_int(ud, UDC_LEVEL);
        if (lvl <= 0) lvl = lvlLegacy;
        cls  = login_udc_int(ud, UDC_CLASS);
        nat  = login_udc_int(ud, UDC_NATION);
        rank = login_udc_int(ud, UDC_RANK);
        mapId = login_udc_int(ud, UDC_MAP);
        logged = login_udc_int(ud, UDC_LOGGED);
        login_udc_str(ud, UDC_GUILD, UDC_GUILDLEN, guild, sizeof guild);
    }

    if (gold == 0 && !name[0] && lvl <= 0) {
        login_log(out, "%s;user=%s;login=ok;char=?;nivel=?;ouro=0;verificacao=STRUTURA_VAZIA\n", t, a->user);
        return 0;
    }

    login_log(out, "%s;user=%s;char=%s;nivel=%d;ouro=%u;classe=%d;nacao=%d;rank=%d;mapa=%d;guild=%s;onlogged=%d;src=%s;verificacao=OK\n",
              t, a->user, name, lvl, gold, cls, nat, rank, mapId, guild[0] ? guild : "-", logged, src);
    return 1;
}

/* ── Console em tempo real: espelha os logs (arquivo + console) ── */
void login_set_console(FILE* c) { g_console = c; }

void login_log(FILE* f, const char* fmt, ...) {
    va_list ap;
    if (f) { va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); }
    if (g_console) { va_start(ap, fmt); vfprintf(g_console, fmt, ap); va_end(ap); }
}

/* loga em `f` (arquivo) e no console, se setado */
static void lf(FILE* f, const char* fmt, ...) {
    va_list ap;
    if (f) { va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); }
    if (g_console) { va_start(ap, fmt); vfprintf(g_console, fmt, ap); va_end(ap); }
}

/* ── Modo de mapeamento: passa o mouse sobre cada botao e aperta F9 ── */
static const char* const MAP_LABELS[] = {
    "entrar", "servidor", "canal", "comeca",
    "selecionar_servidor", "sim", "desconectar"
};
#define MAP_N (sizeof(MAP_LABELS) / sizeof(MAP_LABELS[0]))

int login_map_buttons(void) {
    if (g_console) fprintf(g_console,
        "[MAP] Modo de mapeamento:\n"
        "      Passe o MOUSE sobre cada botao e aperte F9, na ordem:\n"
        "      entrar -> servidor -> canal -> comeca -> sim -> desconectar\n"
        "      (F10 cancela)\n");
    int mapped = 0;
    for (unsigned i = 0; i < MAP_N; i++) {
        if (g_console) fprintf(g_console, "[MAP] (%d/%d) F9 sobre: %s\n",
                               (int)(i + 1), (int)MAP_N, MAP_LABELS[i]);
        for (;;) {
            if (GetAsyncKeyState(VK_F9) & 1) break;         /* marca */
            if (GetAsyncKeyState(VK_F10) & 1) return mapped; /* cancela */
            Sleep(60);
        }
        POINT pt;
        GetCursorPos(&pt);
        if (g_console) fprintf(g_console, "[MAP] %s = (%d,%d)\n", MAP_LABELS[i], pt.x, pt.y);
        /* acumula numa string p/ escrever tudo no login.cfg depois */
        char line[64];
        sprintf_s(line, sizeof(line), "%s %d %d\n", MAP_LABELS[i], pt.x, pt.y);
        strcat_s(g_mapbuf, sizeof(g_mapbuf), line);
        mapped++;
        Sleep(200);
    }
    if (mapped > 0 && g_mapbuf[0]) {
        char path[MAX_PATH];
        sprintf_s(path, MAX_PATH, "D:\\projeto\\CABAL_Login\\login.cfg");
        FILE* f = NULL;
        fopen_s(&f, path, "w");
        if (f) { fputs(g_mapbuf, f); fclose(f); }
    }
    if (g_console) fprintf(g_console, "[MAP] pronto — %d botao(s) gravado(s) em login.cfg\n", mapped);
    g_mapbuf[0] = 0;
    return mapped;
}

bool login_do_credentials(const LoginAccount* a) {
    if (!a || !a->user[0] || !a->pass[0]) return false;
    /* limpa os campos (Ctrl+A + Backspace) ANTES de digitar: ao trocar de
     * conta na MESMA janela, o campo ainda tem o usuario da conta anterior. */
    key_combo(VK_CONTROL, 'A'); send_vk(VK_BACK);
    login_type_text(a->user, 30);          /* 1: id */
    login_press_tab();
    Sleep(200);
    key_combo(VK_CONTROL, 'A'); send_vk(VK_BACK);
    login_type_text(a->pass, 30);          /* 2: senha */
    Sleep(500);
    login_press_enter();                   /* 3: Enter (scancode) */
    Sleep(400);
    login_press_enter();
    Sleep(2000);   /* 2s de respiro antes da proxima acao (selecao de servidor) */
    /* (removido: clique de coordenadas no Entrar — so Enter/teclado) */
    return true;
}

/* nome legivel de uma classificacao de falha da recvwatch */
static const char* fail_reason(RecvWatchClass c) {
    switch (c) {
    case RW_CLASS_WRONGPASS: return "senha_invalida";
    case RW_CLASS_BLOCKED:   return "conta_bloqueada";
    case RW_CLASS_FULL:      return "servidor_cheio";
    case RW_CLASS_MAINT:     return "manutencao";
    case RW_CLASS_OTHER:     return "mensagem_sistema";
    default:                 return "desconhecido";
    }
}

/* loga a causa da falha (classificacao da recvwatch + texto observado). */
static void report_fail(FILE* log, const char* step) {
    RecvWatchClass c = recvwatch_classify();
    char msg[160] = { 0 };
    recvwatch_last_msg(msg, sizeof msg);
    lf(log, "[login] FALHA(%s): %s%s%s\n", step, fail_reason(c),
       msg[0] ? " msg=" : "", msg[0] ? msg : "");
    recvwatch_dump(log);
}

bool login_run_account(const LoginAccount* a, FILE* log) {
    lf(log, "[login] ==== CONTA %s ====\n", a->user);
    lf(g_console, "\n========== CONTA: %s ==========\n", a->user);

    /* 1-3: id + senha + Enter */
    lf(log, "[login] 1-3: digitando id/senha + Enter\n");
    lf(g_console, "[login] 1-3: Digitando usuario/senha + Enter...\n");
    recvwatch_reset();
    login_do_credentials(a);
    if (login_wait_logged_in(20000)) {
        lf(log, "[login] ok: conectado (login)\n");
        lf(g_console, "[login] OK: Logado no servidor de login\n");
    } else {
        lf(g_console, "[login] FALHA: Nao logou no servidor de login\n");
        report_fail(log, "login");
        return false;
    }

    /* 4: Seleção de servidor (Venus) — clique direto na coordenada */
    lf(log, "[login] 4: selecionando servidor VENUS\n");
    lf(g_console, "[login] 4: Selecionando VENUS em (%d,%d)...\n", g_pos_servidor_x, g_pos_servidor_y);
    if (!login_select_server()) {
        lf(g_console, "[login] FALHA: Nao conseguiu clicar em Venus\n");
        return false;
    }
    Sleep(2000);
    if (!login_wait_logged_in(5000)) {
        lf(g_console, "[login] FALHA: Servidor nao confirmou conexao apos Venus\n");
        return false;
    }
    lf(g_console, "[login] OK: Servidor Venus confirmado\n");

    /* 5: Seleção de canal (Canal 9 em Venus) — clique no canal + Enter */
    lf(log, "[login] 5: selecionando CANAL 9\n");
    lf(g_console, "[login] 5: Selecionando CANAL 9...\n");
    if (!login_select_channel()) {
        lf(g_console, "[login] FALHA: Nao conseguiu selecionar canal\n");
        return false;
    }
    if (!login_wait_char_ready(30000)) {
        lf(g_console, "[login] FALHA: Tela de personagem nao apareceu\n");
        report_fail(log, "personagem");
        return false;
    }
    lf(log, "[login] ok   tela de personagem (char listo)\n");
    lf(g_console, "[login] OK: Tela de personagem carregada\n");
    Sleep(2000);

    /* 6: Clicar ENTRAR (confirmar entrada no mundo) */
    lf(log, "[login] 6: clicando ENTRAR\n");
    lf(g_console, "[login] 6: Clicando ENTRAR em (%d,%d)...\n", g_pos_entrar_x, g_pos_entrar_y);
    if (g_pos_entrar_x >= 0 && g_pos_entrar_y >= 0) {
        login_click(g_pos_entrar_x, g_pos_entrar_y);
        Sleep(2000);
    } else {
        lf(g_console, "[login] AVISO: pos_entrar nao configurado, usando Enter\n");
        login_press_enter();
        Sleep(2000);
    }
    lf(g_console, "[login] OK: Entrar clicado\n");

    /* 7: COMEÇA (Iniciar personagem) — abre janela de subsenha */
    lf(log, "[login] 7: clicando COMEÇA (Iniciar)\n");
    lf(g_console, "[login] 7: Clicando COMEÇA em (%d,%d)...\n", g_pos_comeca_x, g_pos_comeca_y);
    if (!login_start_character()) {
        lf(g_console, "[login] FALHA: Nao conseguiu clicar em Começa\n");
        return false;
    }
    recvwatch_reset();
    lf(g_console, "[login] OK: Começa clicado, aguardando janela de subsenha...\n");
    Sleep(2500);

    /* 8-9: Subsenha — digita e confirma */
    if (a->subsenha[0]) {
        lf(log, "[login] 8-9: digitando subsenha\n");
        lf(g_console, "[login] 8-9: Digitando subsenha...\n");
        login_type_text(a->subsenha, 30);
        Sleep(150);
        login_press_enter();
        lf(g_console, "[login] OK: Subsenha enviada\n");
    } else {
        lf(g_console, "[login] 8-9: Sem subsenha configurada, pulando\n");
    }

    /* VALIDAÇÃO: espera DENTRO do mundo */
    lf(g_console, "[login] Aguardando carregar mundo...\n");
    if (login_wait_world(40000)) {
        lf(log, "[login] ok   mundo (char carregado)\n");
        lf(g_console, "[login] OK: Mundo carregado (char no jogo)\n");
    } else {
        lf(g_console, "[login] FALHA: Mundo nao carregou\n");
        report_fail(log, "mundo");
        return false;
    }
    if (login_wait_onlogged(5000)) lf(g_console, "[login] OK: onLogged=1 confirmado\n");

    /* 10-11: abrir inventario (I) */
    lf(log, "[login] 10-11: abrindo inventario (I)\n");
    lf(g_console, "[login] 10-11: Abrindo inventario (I)...\n");
    login_toggle_ui(LOGIN_UI_INVENTORY);
    Sleep(2000);
    lf(g_console, "[login] OK: Inventario aberto\n");

    /* 12-15: menu O -> selecionar servidor -> sim -> desconectar */
    lf(log, "[login] 12-15: menu O + selecionar servidor + sim + desconectar\n");
    lf(g_console, "[login] 12: Menu O...\n");
    login_toggle_ui(LOGIN_UI_MENU);
    Sleep(2000);
    lf(g_console, "[login] 13: Selecionar servidor (funcao direta)...\n");
    login_do_server_select();
    Sleep(2000);
    lf(g_console, "[login] 14: Aguardando Sim...\n");
    Sleep(2500);
    lf(g_console, "[login] 15: Desconectar (funcao direta)...\n");
    login_do_disconnect();
    lf(g_console, "[login] Aguardando voltar para tela de login...\n");
    recvwatch_reset();
    if (login_wait_login_screen(20000)) {
        lf(log, "[login] ok   voltou para login\n");
        lf(g_console, "[login] OK: Voltou para tela de login\n");
    } else {
        lf(g_console, "[login] FALHA: Nao voltou para login\n");
        report_fail(log, "volta_ao_login");
        return false;
    }
    Sleep(2000);

    /* 16: fluxo completo */
    lf(log, "[login] 16: conta finalizada\n");
    lf(g_console, "[login] 16: CONTA FINALIZADA COM SUCESSO\n");
    lf(g_console, "========== FIM: %s ==========\n\n", a->user);
    return true;
}
