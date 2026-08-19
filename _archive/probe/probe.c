/* probe.c — SONDA de observação do dispatch de UI do CabalMain.exe.
 *
 * Hook one-shot em:
 *   - base+0x549FD8 (toggle_ui: abre menu O / inventario I)
 *   - base+0x356B30 (disconnect: fecha as duas conexoes — botao "desconectar")
 *   - base+0x389698 (ENCRYPT: captura SEND plaintext)
 *   - base+0xA01B34 (Canal 0x8C builder — Mercury/Venus/canal)
 *   - base+0x5643A3 (EnterWorld 0x8E builder #1)
 *   - base+0x7222B3 (EnterWorld 0x8E builder #2)
 *   - base+0x80A0B2 (EnterWorld 0x8E builder #3)
 *   - base+0xAC7267 (EnterWorld 0x8E builder #4 — cluster)
 *
 * Quando o jogo invoca a funcao, o stub asm captura return address + pilha e
 * chama o notifier aqui, que loga a cadeia de chamada (o dispatch do clique em
 * assembly) em D:\projeto\CABAL_Login\probe.log. Depois restaura o prologo
 * original e re-entra na funcao (one-shot — o jogo continua normal).
 *
 * Anti-hack bypass: patch 0x352CDB (jne->jmp) IGUAL ao login_dll.c/proxy.
 * DEVE rodar APOS o .text decifrar (Sleep 6000 antes).
 * Protegido por SEH — nunca derruba o jogo.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* stubs asm (hookstub.asm) */
extern void HookEntryDisc(void);
extern void HookEntryTog(void);
extern void HookEntryEnc(void);
extern void HookEntryChan(void);      // 0xA01B34 - Canal 0x8C
extern void HookEntryEW1(void);       // 0x5643A3 - EnterWorld 0x8E #1
extern void HookEntryEW2(void);       // 0x7222B3 - EnterWorld 0x8E #2
extern void HookEntryEW3(void);       // 0x80A0B2 - EnterWorld 0x8E #3
extern void HookEntryEW4(void);       // 0xAC7267 - EnterWorld 0x8E #4
extern void* g_disc_target;
extern void* g_tog_target;
extern void* g_enc_target;
extern void* g_chan_target;
extern void* g_ew1_target;
extern void* g_ew2_target;
extern void* g_ew3_target;
extern void* g_ew4_target;

/* ── Globals ── */
static BYTE* g_base = NULL;
static BYTE g_disc_original[6];
static BYTE g_tog_original[8];
static BYTE g_enc_original[8];
static BYTE g_chan_original[8];
static BYTE g_ew1_original[8];
static BYTE g_ew2_original[8];
static BYTE g_ew3_original[8];
static BYTE g_ew4_original[8];
static FILE* g_log = NULL;
static CRITICAL_SECTION g_lock;

/* ── Anti-hack bypass (IDÊNTICO ao login_dll.c) ──
 * Patch jne rel32 (0F 85) -> jmp rel32 (E9) no mesmo alvo.
 * 0x352CDB = bypass principal (o mesmo que o proxy/login usam).
 * 0x34D1AD = bypass secundário (opcional, desligado por padrão).
 * Protegido por SEH — nunca derruba o jogo. */
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
        /* não deixa um AV no patch derrubar o jogo */
    }
}

static void patch_antihack(void) {
    if (!g_base) return;
    patch_one((uintptr_t)g_base, 0x352CDB);   /* bypass principal — OBRIGATÓRIO */
    /* patch_one((uintptr_t)g_base, 0x34D1AD);  /* bypass secundário — descomente se precisar */
}

static void probe_log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    EnterCriticalSection(&g_lock);
    if (g_log) vfprintf(g_log, fmt, ap);
    LeaveCriticalSection(&g_lock);
    va_end(ap);
}

static void log_addr(const char* tag, void* addr) {
    probe_log("  %-9s %p   (base+%06X)\n", tag, addr,
              (DWORD)((BYTE*)addr - g_base));
}

/* Caminha a cadeia de rbp + varre a pilha crua por enderecos do modulo. */
static void walk_stack(void* entry_rbp, void* entry_rsp) {
    uintptr_t rbp = (uintptr_t)entry_rbp;
    probe_log("-- cadeia rbp:\n");
    for (int i = 0; i < 12 && rbp; i++) {
        uintptr_t ret = 0;
        __try { ret = *(uintptr_t*)(rbp + 8); } __except (EXCEPTION_EXECUTE_HANDLER) { break; }
        if (ret >= (uintptr_t)g_base && ret < (uintptr_t)g_base + 0x2000000)
            log_addr("frame", (void*)ret);
        __try { rbp = *(uintptr_t*)rbp; } __except (EXCEPTION_EXECUTE_HANDLER) { break; }
    }
    probe_log("-- pilha (qwords com endereco do modulo, [rsp, rsp+0x120]):\n");
    uintptr_t p = (uintptr_t)entry_rsp;
    for (int i = 0; i < 0x120 / 8; i++) {
        uintptr_t v = 0;
        __try { v = *(uintptr_t*)(p + i * 8); } __except (EXCEPTION_EXECUTE_HANDLER) { break; }
        if (v >= (uintptr_t)g_base && v < (uintptr_t)g_base + 0x2000000)
            log_addr("stack", (void*)v);
    }
}

/* restaura os bytes originais (one-shot) e devolve 1 se restaurou */
static int restore_target(void* target, BYTE* original, size_t n) {
    __try {
        DWORD old;
        if (!VirtualProtect(target, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
        memcpy(target, original, n);
        VirtualProtect(target, n, old, &old);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

__declspec(noinline) void __cdecl probe_notify_disc(void* retaddr, void* this_ptr,
                                                    void* entry_rsp, void* entry_rbp) {
    probe_log("\n===== DISCONNECT (base+0x356B30) DISPARADO =====\n");
    log_addr("retaddr", retaddr);
    probe_log("  this     %p\n", this_ptr);
    probe_log("  entry_rsp %p  entry_rbp %p\n", entry_rsp, entry_rbp);
    walk_stack(entry_rbp, entry_rsp);
    restore_target(g_disc_target, g_disc_original, 6);
    probe_log("== prologo restaurado (one-shot) — proximas chamadas nao capturam ==\n");
    if (g_log) fflush(g_log);
}

__declspec(noinline) void __cdecl probe_notify_tog(void* retaddr, void* this_ptr,
                                                   void* entry_rsp, void* entry_rbp) {
    probe_log("\n===== TOGGLE_UI (base+0x549FD8) DISPARADO =====\n");
    log_addr("retaddr", retaddr);
    probe_log("  this     %p\n", this_ptr);
    probe_log("  entry_rsp %p  entry_rbp %p\n", entry_rsp, entry_rbp);
    walk_stack(entry_rbp, entry_rsp);
    restore_target(g_tog_target, g_tog_original, 8);
    probe_log("== prologo restaurado (one-shot) ==\n");
    if (g_log) fflush(g_log);
}

__declspec(noinline) void __cdecl probe_notify_enc(void* retaddr, void* this_ptr,
                                                    void* entry_rsp, void* entry_rbp) {
    probe_log("\n===== ENCRYPT (base+0x389698) DISPARADO =====\n");
    log_addr("retaddr", retaddr);
    probe_log("  this     %p\n", this_ptr);
    probe_log("  entry_rsp %p  entry_rbp %p\n", entry_rsp, entry_rbp);
    walk_stack(entry_rbp, entry_rsp);
    restore_target(g_enc_target, g_enc_original, 8);
    probe_log("== prologo restaurado (one-shot) ==\n");
    if (g_log) fflush(g_log);
}

__declspec(noinline) void __cdecl probe_notify_chan(void* retaddr, void* this_ptr,
                                                     void* entry_rsp, void* entry_rbp) {
    probe_log("\n===== CANAL 0x8C (base+0xA01B34) DISPARADO =====\n");
    log_addr("retaddr", retaddr);
    probe_log("  this     %p\n", this_ptr);
    probe_log("  entry_rsp %p  entry_rbp %p\n", entry_rsp, entry_rbp);
    walk_stack(entry_rbp, entry_rsp);
    restore_target(g_chan_target, g_chan_original, 8);
    probe_log("== prologo restaurado (one-shot) ==\n");
    if (g_log) fflush(g_log);
}

__declspec(noinline) void __cdecl probe_notify_ew1(void* retaddr, void* this_ptr,
                                                    void* entry_rsp, void* entry_rbp) {
    probe_log("\n===== ENTERWORLD 0x8E #1 (base+0x5643A3) DISPARADO =====\n");
    log_addr("retaddr", retaddr);
    probe_log("  this     %p\n", this_ptr);
    probe_log("  entry_rsp %p  entry_rbp %p\n", entry_rsp, entry_rbp);
    walk_stack(entry_rbp, entry_rsp);
    restore_target(g_ew1_target, g_ew1_original, 8);
    probe_log("== prologo restaurado (one-shot) ==\n");
    if (g_log) fflush(g_log);
}

__declspec(noinline) void __cdecl probe_notify_ew2(void* retaddr, void* this_ptr,
                                                    void* entry_rsp, void* entry_rbp) {
    probe_log("\n===== ENTERWORLD 0x8E #2 (base+0x7222B3) DISPARADO =====\n");
    log_addr("retaddr", retaddr);
    probe_log("  this     %p\n", this_ptr);
    probe_log("  entry_rsp %p  entry_rbp %p\n", entry_rsp, entry_rbp);
    walk_stack(entry_rbp, entry_rsp);
    restore_target(g_ew2_target, g_ew2_original, 8);
    probe_log("== prologo restaurado (one-shot) ==\n");
    if (g_log) fflush(g_log);
}

__declspec(noinline) void __cdecl probe_notify_ew3(void* retaddr, void* this_ptr,
                                                    void* entry_rsp, void* entry_rbp) {
    probe_log("\n===== ENTERWORLD 0x8E #3 (base+0x80A0B2) DISPARADO =====\n");
    log_addr("retaddr", retaddr);
    probe_log("  this     %p\n", this_ptr);
    probe_log("  entry_rsp %p  entry_rbp %p\n", entry_rsp, entry_rbp);
    walk_stack(entry_rbp, entry_rsp);
    restore_target(g_ew3_target, g_ew3_original, 8);
    probe_log("== prologo restaurado (one-shot) ==\n");
    if (g_log) fflush(g_log);
}

__declspec(noinline) void __cdecl probe_notify_ew4(void* retaddr, void* this_ptr,
                                                    void* entry_rsp, void* entry_rbp) {
    probe_log("\n===== ENTERWORLD 0x8E #4 (base+0xAC7267) DISPARADO =====\n");
    log_addr("retaddr", retaddr);
    probe_log("  this     %p\n", this_ptr);
    probe_log("  entry_rsp %p  entry_rbp %p\n", entry_rsp, entry_rbp);
    walk_stack(entry_rbp, entry_rsp);
    restore_target(g_ew4_target, g_ew4_original, 8);
    probe_log("== prologo restaurado (one-shot) ==\n");
    if (g_log) fflush(g_log);
}

/* verifica se prologo tem instrucao RIP-relative (mov rax,[rip+disp32] ou jmp [rip+disp32]).
 * se tiver, nao podemos relocar o prologo com jmp simples. */
static int has_rip_relative(const unsigned char* b, int n) {
    static const unsigned char mr[8] = { 0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D };
    for (int i = 0; i + 2 < n; ++i) {
        if (b[i] == 0xFF && b[i + 1] == 0x25) return 1;           // jmp/call [rip+disp32]
        if (b[i] == 0x48 && b[i + 1] == 0x8B)                     // mov rax,[rip+disp32]
            for (int k = 0; k < 8; ++k) if (b[i + 2] == mr[k]) return 1;
        if (b[i] == 0x4C && b[i + 1] == 0x8B)                     // mov r8-r15,[rip+disp32]
            for (int k = 0; k < 8; ++k) if (b[i + 2] == mr[k]) return 1;
    }
    return 0;
}

/* Instala jmp rel32 (5 bytes) em base+rva -> hookentry. Salva o prologo.
 * NAO hooka se prologo tem RIP-relative (crasharia ao restaurar).
 * NAO hooka se prologo ja modificado (ret, int3, jmp, call). */
static void install_hook(DWORD rva, void* hookentry, BYTE* save, size_t save_len,
                         void** out_target) {
    BYTE* t = g_base + rva;
    DWORD old;
    __try {
        if (!VirtualProtect(t, 16, PAGE_EXECUTE_READWRITE, &old)) {
            probe_log("ERRO: VirtualProtect em base+%06X\n", rva);
            return;
        }
        /* le prologo original pra validacao e log */
        BYTE orig[16];
        memcpy(orig, t, save_len);
        probe_log("prologo base+%06X: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                  rva, orig[0], orig[1], orig[2], orig[3], orig[4], orig[5], orig[6], orig[7]);

        /* verifica prologo invalido: ret (C3), int3 (CC), jmp (E9/EB), call (E8), jmp [rip] (FF 25), RIP-relative */
        if (orig[0] == 0xC3 || orig[0] == 0xCC || orig[0] == 0xE9 || orig[0] == 0xEB ||
            orig[0] == 0xE8 || (orig[0] == 0xFF && orig[1] == 0x25) || has_rip_relative(orig, save_len)) {
            probe_log("PULADO: base+%06X prologo invalido/RIP-relative\n", rva);
            VirtualProtect(t, 16, old, &old);
            return;
        }

        memcpy(save, t, save_len);
        t[0] = 0xE9;
        *(int*)(t + 1) = (int)((BYTE*)hookentry - (t + 5));
        VirtualProtect(t, 16, old, &old);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        probe_log("ERRO: hook em base+%06X\n", rva);
        return;
    }
    *out_target = t;
    probe_log("hook instalado: base+%06X -> %p\n", rva, hookentry);
}

static DWORD WINAPI probe_thread(LPVOID lp) {
    (void)lp;
    Sleep(6000);                       /* espera o .text do Themida decifrar */
    g_base = (BYTE*)GetModuleHandleA("CabalMain.exe");
    if (!g_base) return 0;

    /* Anti-hack bypass (DEVE rodar DEPOIS do g_base estar setado, com .text decifrado) */
    patch_antihack();

    fopen_s(&g_log, "D:\\projeto\\CABAL_Login\\probe.log", "a");
    if (g_log) setvbuf(g_log, NULL, _IONBF, 0);
    InitializeCriticalSection(&g_lock);
    probe_log("\n## sonda carregada, base=%p\n", g_base);
    probe_log("## anti-hack patch aplicado em 0x352CDB\n");
    install_hook(0x549FD8, (void*)HookEntryTog, g_tog_original, 8, &g_tog_target);
    install_hook(0x356B30, (void*)HookEntryDisc, g_disc_original, 6, &g_disc_target);
    install_hook(0x389698, (void*)HookEntryEnc, g_enc_original, 8, &g_enc_target);

    /* NOVOS: builders de canal (0x8C) e EnterWorld (0x8E) */
    install_hook(0xA01B34, (void*)HookEntryChan, g_chan_original, 8, &g_chan_target);   // Canal 0x8C builder
    install_hook(0x5643A3, (void*)HookEntryEW1, g_ew1_original, 8, &g_ew1_target);      // EnterWorld 0x8E #1
    install_hook(0x7222B3, (void*)HookEntryEW2, g_ew2_original, 8, &g_ew2_target);      // EnterWorld 0x8E #2
    install_hook(0x80A0B2, (void*)HookEntryEW3, g_ew3_original, 8, &g_ew3_target);      // EnterWorld 0x8E #3
    install_hook(0xAC7267, (void*)HookEntryEW4, g_ew4_original, 8, &g_ew4_target);      // EnterWorld 0x8E #4

    probe_log("pronto. Acoes a capturar:\n");
    probe_log("  [O=menu] [I=inventario] [menu->desconectar] [ENCRYPT=send plaintext]\n");
    probe_log("  [Mercury/Venus=canal 0x8C] [Iniciar=EnterWorld 0x8E]\n");
    if (g_log) fflush(g_log);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD reason, LPVOID lpReserved) {
    (void)hModule; (void)lpReserved;
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE t = CreateThread(NULL, 0, probe_thread, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}