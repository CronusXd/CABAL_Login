/* recvwatch.c — observa o RECV do cliente e decripta com a keychain XOR do
 * Cabal (porta exata do CABALREVERSE PacketManager) para classificar falhas
 * de login (senha errada / bloqueada / cheio / manutencao).
 *
 * Como entra: hook no IAT do CabalMain.exe para ws2_32!recv e, se existir,
 * ws2_32!WSARecv — sem patchar codigo, sem relocar prologo. Se o jogo for
 * protegido (IAT em modo protegido) o hook falha silencioso (nao regride).
 *
 * O stream recebido esta CIFRADO: cada pacote comeca com magic 0xB7E2 (ou
 * 0xC8F3 extended, que ignoramos) e o dword 0 e XOR com SEND_XORKEY. Os
 * dwords seguintes usam keytable[raw_dword & 0x3FFF] (1a metade, igual ao
 * DecodePacket do jogo). Validamos com o harness tests/test_crypto.c.
 */

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "recvwatch.h"

#define RW_RECV_KEY  0x7AB38CF1
#define RW_SEED      (0x8F54C37B | 1)
#define RW_KEYNUM    16384
#define RW_MAGIC     0xB7E2
#define RW_MAGIC_EXT 0xC8F3

/* ── keychain (CKeyRand::Rand, 32-bit modular) ───────────────────────────── */
static unsigned int g_hold;
static unsigned int g_keytable[RW_KEYNUM];
static int g_keyready = 0;

static void ck_seed(unsigned int s) { g_hold = s; }

static int ck_rand(void) {
    g_hold = g_hold * 0x2F6B6F5u + 0x14698B7u;
    return (int)((((g_hold >> 0x10) * 0x27F41C3u + 0xB327BDu) >> 0x10));
}

static void ensure_keys(void) {
    if (g_keyready) return;
    ck_seed(RW_SEED);
    for (int i = 0; i < RW_KEYNUM; ++i) {
        unsigned int wl = (unsigned short)ck_rand();
        unsigned int wh = (unsigned short)ck_rand();
        g_keytable[i] = (wl & 0xFFFF) | ((wh & 0xFFFF) << 16);
    }
    g_keyready = 1;
}

/* decripta um bloco de `len` bytes (DecodePacket); avanca o chain pelo low14
 * do dword cru (que eh o que o jogo ve no socket). */
static void rw_decrypt(unsigned char* buf, int len) {
    unsigned int dwXorKey = RW_RECV_KEY;
    for (int i = 0; i < len / 4; ++i) {
        unsigned int* pw = (unsigned int*)(buf + i * 4);
        unsigned int raw = *pw;
        unsigned int old = raw & 0x3FFF;
        *pw = raw ^ dwXorKey;
        dwXorKey = g_keytable[old];
    }
    int tail = len & 3;
    if (tail) {
        static const unsigned int mask[4] = { 0xFFFFFFFF, 0xFFFFFF00, 0xFFFF0000, 0xFF000000 };
        unsigned char* tp = buf + (len / 4) * 4;
        unsigned int result = dwXorKey & ~mask[tail];
        for (int k = 0; k < tail; ++k)
            tp[k] ^= (unsigned char)(result >> (8 * k));
    }
}

/* ── estado observado ─────────────────────────────────────────────────────── */
#define RW_CMDMAX  256
#define RW_SOCKMAX 8
#define RW_SOCKBUF 0x10000

static CRITICAL_SECTION g_cs;
static int g_cs_ready = 0;

typedef struct {
    SOCKET s;
    int    used;
    unsigned char data[RW_SOCKBUF];
} RWSock;

static RWSock   g_socks[RW_SOCKMAX];
static unsigned short g_cmd[RW_CMDMAX];
static int      g_cmdlen[RW_CMDMAX];
static int      g_cmdn = 0;
static char     g_lastmsg[128] = { 0 };
static RecvWatchClass g_class = RW_CLASS_NONE;
static unsigned char g_scratch[RW_SOCKBUF];

/* ── classificacao por conteudo da mensagem ──────────────────────────────── */
static int looks_like(const char* hay_asis, int asislen,
                      const char* hay_wide, int widelen,
                      const char* needle) {
    /* busca case-insensitive em ambos os encodings (ascii e utf16le) */
    size_t n = strlen(needle);
    for (int i = 0; i + (int)n <= asislen; ++i)
        if (!_strnicmp(hay_asis + i, needle, n)) return 1;
    if (widelen >= (int)n * 2) {
        for (int i = 0; i + (int)n * 2 <= widelen; i += 2) {
            int ok = 1;
            for (int j = 0; j < (int)n; ++j) {
                char lo = hay_wide[i + j * 2];
                char hi = hay_wide[i + j * 2 + 1];
                if (hi != 0) { ok = 0; break; }
                if (lo != needle[j] && lo != needle[j] - 32) { ok = 0; break; }
            }
            if (ok) return 1;
        }
    }
    return 0;
}

static void note_message(const unsigned char* payload, int plen) {
    /* extrai a primeira run ASCII impressível >= 4 como "ultima mensagem" */
    int start = -1, best = -1, best_len = 0;
    for (int i = 0; i < plen; ++i) {
        unsigned char c = payload[i];
        int pr = (c >= 32 && c < 127);
        if (pr && start < 0) start = i;
        if (!pr && start >= 0) {
            if (i - start >= 4 && i - start > best_len) { best = start; best_len = i - start; }
            start = -1;
        }
    }
    if (start >= 0 && plen - start >= 4 && plen - start > best_len) { best = start; best_len = plen - start; }
    if (best >= 0 && best_len > 0) {
        char* m = g_lastmsg;
        int n = best_len < (int)sizeof(g_lastmsg) - 1 ? best_len : (int)sizeof(g_lastmsg) - 1;
        memcpy(m, payload + best, n);
        m[n] = 0;
    }

    char asis[512]; int asislen = 0;
    char wide[512]; strcpy_s(wide, sizeof wide, ""); int widelen = 0;
    for (int i = 0; i < plen && asislen < (int)sizeof asis - 1; ++i) {
        unsigned char c = payload[i];
        if (c >= 32 && c < 127) asis[asislen++] = (char)c;
        else if (asislen && asis[asislen - 1] != ' ') asis[asislen++] = ' ';
    }
    asis[asislen] = 0;
    if (plen < (int)sizeof wide - 1) {
        memcpy(wide, payload, plen);
        widelen = plen;
    }

    if (looks_like(asis, asislen, wide, widelen, "pass") ||
        looks_like(asis, asislen, wide, widelen, "senha") ||
        looks_like(asis, asislen, wide, widelen, "incorret"))
        g_class = RW_CLASS_WRONGPASS;
    else if (looks_like(asis, asislen, wide, widelen, "block") ||
             looks_like(asis, asislen, wide, widelen, "suspend") ||
             looks_like(asis, asislen, wide, widelen, "bloquead") ||
             looks_like(asis, asislen, wide, widelen, "ban"))
        g_class = RW_CLASS_BLOCKED;
    else if (looks_like(asis, asislen, wide, widelen, "full") ||
             looks_like(asis, asislen, wide, widelen, "lotad") ||
             looks_like(asis, asislen, wide, widelen, "cheio"))
        g_class = RW_CLASS_FULL;
    else if (looks_like(asis, asislen, wide, widelen, "maintenance") ||
             looks_like(asis, asislen, wide, widelen, "manuten"))
        g_class = RW_CLASS_MAINT;
    else if (g_class == RW_CLASS_NONE)
        g_class = RW_CLASS_OTHER;
}

/* registra um pacote completo (ainda cifrado) ja validado por magic. */
static void record_packet(const unsigned char* enc, int pktlen, SOCKET s) {
    (void)s;
    if (pktlen > (int)sizeof g_scratch) return;
    memcpy(g_scratch, enc, pktlen);
    rw_decrypt(g_scratch, pktlen);

    unsigned short maincmd = 0;
    memcpy(&maincmd, g_scratch + 4, 2);   /* sS2C_HEADER: magic, len, maincmd */
    if (g_cmdn < RW_CMDMAX) {
        g_cmd[g_cmdn] = maincmd;
        g_cmdlen[g_cmdn] = pktlen;
        g_cmdn++;
    }

    /* 0x78 = NFY_SYSTEMMESSG, 0x79 = NFY_SERVERSTATE (falhas de login) */
    if (maincmd == 0x78 || maincmd == 0x79) {
        int plen = pktlen - 6;
        if (plen > 0) note_message(g_scratch + 6, plen);
    }
}

/* observa um bloco recebido (pode conter 0..N pacotes, completo ou nao). */
static void observe(SOCKET s, const unsigned char* data, int len) {
    RWSock* slot = NULL;
    for (int i = 0; i < RW_SOCKMAX; ++i) {
        if (g_socks[i].s == s) { slot = &g_socks[i]; break; }
        if (!slot && g_socks[i].s == INVALID_SOCKET) slot = &g_socks[i];
    }
    if (!slot) return;                      /* sem vaga: nao rastreia este socket */
    if (slot->s == INVALID_SOCKET) { slot->s = s; slot->used = 0; }

    if (len <= 0) return;
    if (slot->used + len > (int)sizeof slot->data) {
        /* buffer estourou (stream comprida demais): recomeça a reassemblagem */
        slot->used = 0;
        if (len > (int)sizeof slot->data) return;
    }
    memcpy(slot->data + slot->used, data, len);
    slot->used += len;

    /* tenta consumir pacotes do inicio do buffer */
    while (slot->used >= 4) {
        unsigned int raw0;
        memcpy(&raw0, slot->data, 4);
        unsigned int dec0 = raw0 ^ RW_RECV_KEY;
        unsigned int magic = dec0 & 0xFFFF;
        if (magic == RW_MAGIC_EXT) break;            /* extended: sem parse, segura */
        if (magic != RW_MAGIC) {                     /* nao-comeco -> descarta 1 byte */
            memmove(slot->data, slot->data + 1, slot->used - 1);
            slot->used--;
            continue;
        }
        unsigned int pktlen = dec0 >> 16;
        if (pktlen < 6 || pktlen > sizeof slot->data) { /* invalido -> descarta 1 */
            memmove(slot->data, slot->data + 1, slot->used - 1);
            slot->used--;
            continue;
        }
        if ((unsigned int)slot->used < pktlen) break;    /* parcelado: espera mais */
        record_packet(slot->data, (int)pktlen, s);
        memmove(slot->data, slot->data + pktlen, slot->used - (int)pktlen);
        slot->used -= (int)pktlen;
    }
}

/* ── hooks ───────────────────────────────────────────────────────────────── */
typedef int(WSAAPI* recv_fn)(SOCKET s, char* buf, int len, int flags);
typedef int(WSAAPI* wsarecv_fn)(SOCKET s, LPWSABUF lb, DWORD cnt, LPDWORD got,
                                LPDWORD fl, LPWSAOVERLAPPED ov,
                                LPWSAOVERLAPPED_COMPLETION_ROUTINE cr);

static recv_fn    real_recv = NULL;
static wsarecv_fn real_wsarecv = NULL;

static int WSAAPI recv_hook(SOCKET s, char* buf, int len, int flags) {
    int rc = real_recv ? real_recv(s, buf, len, flags) : SOCKET_ERROR;
    if (rc > 0 && g_cs_ready) {
        EnterCriticalSection(&g_cs);
        observe(s, (const unsigned char*)buf, rc);
        LeaveCriticalSection(&g_cs);
    }
    return rc;
}

static int WSAAPI wsarecv_hook(SOCKET s, LPWSABUF lb, DWORD cnt, LPDWORD got,
                               LPDWORD fl, LPWSAOVERLAPPED ov,
                               LPWSAOVERLAPPED_COMPLETION_ROUTINE cr) {
    int rc = real_wsarecv ? real_wsarecv(s, lb, cnt, got, fl, ov, cr) : SOCKET_ERROR;
    if (rc == 0 && got && *got && !ov && lb && cnt && g_cs_ready) {
        unsigned char tmp[0x2000];   /* pequeno: threads de rede podem ter stack curta */
        int off = 0;
        for (DWORD i = 0; i < cnt && (unsigned)off < sizeof tmp; ++i) {
            int n = lb[i].len;
            if (n > (int)(sizeof tmp - off)) n = (int)(sizeof tmp - off);
            if (n > 0) { memcpy(tmp + off, lb[i].buf, n); off += n; }
        }
        if (off > 0) {
            EnterCriticalSection(&g_cs);
            observe(s, tmp, off);
            LeaveCriticalSection(&g_cs);
        }
    }
    return rc;
}

/* ── instalacao IN-PLACE (trampoline de 12 bytes) ──────────────────────────
 * Necessario porque neste build (Themida) o CabalMain importa de ws2_32
 * SOMENTE `bind` — recv/WSARecv sao resolvidos em runtime (sem IAT). Os
 * prologos de recv/WSARecv (Win10 x64) sao so "mov [rsp+n], reg" nos 12
 * primeiros bytes (sem RIP-relative) — copia pura + jmp absoluto funciona.
 * Preferimos IAT quando existe; in-place e o fallback universal. */

static BYTE*   g_recv_tr = NULL, *g_wsarecv_tr = NULL;
static FARPROC g_recv_addr = NULL, g_wsarecv_addr = NULL;
static BYTE    g_recv_orig[12], g_wsarecv_orig[12];

static BYTE* rw_make_tr(void* orig) {
    BYTE* tr = (BYTE*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) return NULL;
    memcpy(tr, orig, 12);
    tr[12] = 0x48; tr[13] = 0xB8;                                      /* mov rax, imm64 */
    *(UINT64*)(tr + 14) = (UINT64)((BYTE*)orig + 12);
    tr[22] = 0xFF; tr[23] = 0xE0;                                      /* jmp rax */
    return tr;
}

/* prologo contem RIP-relative (48 8B XD / FF 25)? se sim, nao relocamos. */
static int rw_has_rip(const unsigned char* b, int n) {
    static const unsigned char mr[8] = { 0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D };
    for (int i = 0; i + 2 < n; ++i) {
        if (b[i] == 0xFF && b[i + 1] == 0x25) return 1;
        if (b[i] == 0x48 && b[i + 1] == 0x8B)
            for (int k = 0; k < 8; ++k) if (b[i + 2] == mr[k]) return 1;
    }
    return 0;
}

static int rw_patch_inplace(void* fn, void* hook, BYTE** tr_out, BYTE* orig_store, FARPROC* addr_out) {
    unsigned char first[12];
    DWORD old;
    if (!fn || !tr_out) return 0;
    if (!VirtualProtect(fn, 12, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(first, fn, 12);
    if (first[0] == 0xC3 || first[0] == 0xCC || first[0] == 0xE9 || first[0] == 0xEB ||
        first[0] == 0xE8 || (first[0] == 0xFF && first[1] == 0x25) || rw_has_rip(first, 12)) {
        VirtualProtect(fn, 12, old, &old);
        return 0;
    }
    BYTE* tr = rw_make_tr(fn);                    /* copia o prologo ANTES do patch */
    if (!tr) { VirtualProtect(fn, 12, old, &old); return 0; }
    memcpy(first, fn, 12);                        /* rele (ainda original) */
    ((BYTE*)fn)[0] = 0x48; ((BYTE*)fn)[1] = 0xB8; /* 48 B8 <hook64> FF E0 */
    *(UINT64*)((BYTE*)fn + 2) = (UINT64)hook;
    ((BYTE*)fn)[10] = 0xFF; ((BYTE*)fn)[11] = 0xE0;
    VirtualProtect(fn, 12, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 12);
    memcpy(orig_store, first, 12);
    *tr_out = tr;
    *addr_out = (FARPROC)fn;
    return 1;
}

static void rw_patch_restore(FARPROC addr, const BYTE* orig) {
    if (!addr || !orig) return;
    DWORD old;
    if (VirtualProtect((void*)addr, 12, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy((void*)addr, orig, 12);
        VirtualProtect((void*)addr, 12, old, &old);
        FlushInstructionCache(GetCurrentProcess(), (void*)addr, 12);
    }
}

/* ── instalacao via IAT ───────────────────────────────────────────────────── */
static FARPROC iat_hook(HMODULE mod, const char* dll, const char* name, FARPROC hook) {
    IMAGE_DOS_HEADER dos;
    memcpy(&dos, (void*)mod, sizeof dos);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS nt;
    memcpy(&nt, (void*)((BYTE*)mod + dos.e_lfanew), sizeof nt);
    if (nt.Signature != IMAGE_NT_SIGNATURE) return NULL;
    DWORD impRva = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!impRva) return NULL;

    IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)mod + impRva);
    for (; d->Name; d++) {
        const char* dn = (const char*)mod + d->Name;
        if (_stricmp(dn, dll)) continue;
        IMAGE_THUNK_DATA* orig = (IMAGE_THUNK_DATA*)((BYTE*)mod + d->OriginalFirstThunk);
        IMAGE_THUNK_DATA* iat = (IMAGE_THUNK_DATA*)((BYTE*)mod + d->FirstThunk);
        for (; orig->u1.AddressOfData; orig++, iat++) {
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            IMAGE_IMPORT_BY_NAME* ibn = (IMAGE_IMPORT_BY_NAME*)((BYTE*)mod + orig->u1.AddressOfData);
            ULONG_PTR* field = &iat->u1.Function;
            char fname[64];
            strncpy_s(fname, sizeof fname, (char*)ibn->Name, _TRUNCATE);
            if (_stricmp(fname, name)) continue;
            FARPROC origfn = (FARPROC)*field;
            DWORD old;
            if (!VirtualProtect(field, sizeof(*field), PAGE_READWRITE, &old)) {
                /* IAT em regiao protegida (anti-cheat): nao force */
                return NULL;
            }
            *field = (ULONG_PTR)hook;
            VirtualProtect(field, sizeof(*field), old, &old);
            return origfn;
        }
    }
    return NULL;
}

static void iat_restore(HMODULE mod, const char* dll, const char* name, FARPROC hook, FARPROC orig) {
    IMAGE_DOS_HEADER dos;
    memcpy(&dos, (void*)mod, sizeof dos);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return;
    IMAGE_NT_HEADERS nt;
    memcpy(&nt, (void*)((BYTE*)mod + dos.e_lfanew), sizeof nt);
    if (nt.Signature != IMAGE_NT_SIGNATURE) return;
    DWORD impRva = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!impRva) return;
    IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)mod + impRva);
    for (; d->Name; d++) {
        const char* dn = (const char*)mod + d->Name;
        if (_stricmp(dn, dll)) continue;
        IMAGE_THUNK_DATA* origt = (IMAGE_THUNK_DATA*)((BYTE*)mod + d->OriginalFirstThunk);
        IMAGE_THUNK_DATA* iat = (IMAGE_THUNK_DATA*)((BYTE*)mod + d->FirstThunk);
        for (; origt->u1.AddressOfData; origt++, iat++) {
            if (origt->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            IMAGE_IMPORT_BY_NAME* ibn = (IMAGE_IMPORT_BY_NAME*)((BYTE*)mod + origt->u1.AddressOfData);
            char fname[64];
            strncpy_s(fname, sizeof fname, (char*)ibn->Name, _TRUNCATE);
            if (_stricmp(fname, name)) continue;
            if ((FARPROC)iat->u1.Function == hook) {
                DWORD old;
                if (VirtualProtect(&iat->u1.Function, sizeof(iat->u1.Function), PAGE_READWRITE, &old)) {
                    iat->u1.Function = (ULONG_PTR)orig;
                    VirtualProtect(&iat->u1.Function, sizeof(iat->u1.Function), old, &old);
                }
            }
            return;
        }
    }
}

int recvwatch_init(void) {
    ensure_keys();
    if (!g_cs_ready) {
        InitializeCriticalSection(&g_cs);
        g_cs_ready = 1;
    }
    for (int i = 0; i < RW_SOCKMAX; ++i) g_socks[i].s = INVALID_SOCKET;

    HMODULE cabal = GetModuleHandleA("CabalMain.exe");
    HMODULE ws2   = GetModuleHandleA("ws2_32.dll");

    int n = 0;

    /* recv: IAT se o build tem import estatico; senao in-place (este build). */
    FARPROC o1 = cabal ? iat_hook(cabal, "ws2_32.dll", "recv", (FARPROC)recv_hook) : NULL;
    if (o1) { real_recv = (recv_fn)o1; n++; }
    else if (ws2) {
        void* a = (void*)GetProcAddress(ws2, "recv");
        if (rw_patch_inplace(a, (void*)recv_hook, &g_recv_tr, g_recv_orig, &g_recv_addr)) {
            real_recv = (recv_fn)g_recv_tr;
            n++;
        }
    }

    /* WSARecv: mesmo esquema. */
    FARPROC o2 = cabal ? iat_hook(cabal, "ws2_32.dll", "WSARecv", (FARPROC)wsarecv_hook) : NULL;
    if (o2) { real_wsarecv = (wsarecv_fn)o2; n++; }
    else if (ws2) {
        void* a = (void*)GetProcAddress(ws2, "WSARecv");
        if (rw_patch_inplace(a, (void*)wsarecv_hook, &g_wsarecv_tr, g_wsarecv_orig, &g_wsarecv_addr)) {
            real_wsarecv = (wsarecv_fn)g_wsarecv_tr;
            n++;
        }
    }
    return n;
}

void recvwatch_shutdown(void) {
    if (!g_cs_ready) return;
    HMODULE cabal = GetModuleHandleA("CabalMain.exe");
    if (cabal) {
        if (real_recv && !g_recv_tr)
            iat_restore(cabal, "ws2_32.dll", "recv", (FARPROC)recv_hook, (FARPROC)real_recv);
        if (real_wsarecv && !g_wsarecv_tr)
            iat_restore(cabal, "ws2_32.dll", "WSARecv", (FARPROC)wsarecv_hook, (FARPROC)real_wsarecv);
    }
    if (g_recv_tr)    rw_patch_restore(g_recv_addr, g_recv_orig);
    if (g_wsarecv_tr) rw_patch_restore(g_wsarecv_addr, g_wsarecv_orig);
    if (g_recv_tr)    VirtualFree(g_recv_tr, 0, MEM_RELEASE);
    if (g_wsarecv_tr) VirtualFree(g_wsarecv_tr, 0, MEM_RELEASE);
    g_recv_tr = NULL; g_wsarecv_tr = NULL;
    g_recv_addr = NULL; g_wsarecv_addr = NULL;
    real_recv = NULL;                 /* separar: tipos de fn pointers sao distintos */
    real_wsarecv = NULL;
    DeleteCriticalSection(&g_cs);
    g_cs_ready = 0;
}

void recvwatch_reset(void) {
    if (!g_cs_ready) return;
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < RW_SOCKMAX; ++i) { g_socks[i].s = INVALID_SOCKET; g_socks[i].used = 0; }
    g_cmdn = 0;
    g_lastmsg[0] = 0;
    g_class = RW_CLASS_NONE;
    LeaveCriticalSection(&g_cs);
}

RecvWatchClass recvwatch_classify(void) {
    if (!g_cs_ready) return RW_CLASS_NONE;
    RecvWatchClass c;
    EnterCriticalSection(&g_cs);
    c = g_class;
    LeaveCriticalSection(&g_cs);
    return c;
}

void recvwatch_last_msg(char* out, int max) {
    if (!out || max <= 0) return;
    out[0] = 0;
    if (!g_cs_ready) return;
    EnterCriticalSection(&g_cs);
    strncpy_s(out, max, g_lastmsg, _TRUNCATE);
    LeaveCriticalSection(&g_cs);
}

int recvwatch_cmdcount(void) {
    if (!g_cs_ready) return 0;
    int n;
    EnterCriticalSection(&g_cs);
    n = g_cmdn;
    LeaveCriticalSection(&g_cs);
    return n;
}

int recvwatch_cmd_at(int i) {
    if (!g_cs_ready || i < 0 || i >= g_cmdn) return -1;
    int c;
    EnterCriticalSection(&g_cs);
    c = (i < g_cmdn) ? g_cmd[i] : -1;
    LeaveCriticalSection(&g_cs);
    return c;
}

int recvwatch_cmdlen_at(int i) {
    if (!g_cs_ready || i < 0 || i >= g_cmdn) return -1;
    int c;
    EnterCriticalSection(&g_cs);
    c = (i < g_cmdn) ? g_cmdlen[i] : -1;
    LeaveCriticalSection(&g_cs);
    return c;
}

void recvwatch_dump(FILE* f) {
    if (!f) return;
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < g_cmdn; ++i)
        fprintf(f, "    S2C maincmd=%04X len=%d\n", g_cmd[i], g_cmdlen[i]);
    if (g_lastmsg[0]) fprintf(f, "    ultima msg: %s\n", g_lastmsg);
    fprintf(f, "    class=%d\n", (int)g_class);
    LeaveCriticalSection(&g_cs);
}