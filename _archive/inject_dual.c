/* inject_dual.c — Injeta DUAS DLLs no CabalMain.exe no launch
 *
 * Uso: inject_dual.exe <game_exe> <game_args> <dll1> <dll2>
 * Exemplo: inject_dual.exe "C:\ConnectGame\cabal.exe" husky
 *          "D:\projeto\Connect Game\bin\connect_game.dll"
 *          "D:\projeto\CABAL_Login\x64\Release\probe.dll"
 *
 * Compila: cl /O2 /Feinject_dual.exe inject_dual.c
 * Requer: Admin (CreateRemoteThread, VirtualAllocEx)
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>
#include <tchar.h>

#define PROCESS_ALL_ACCESS_X 0x1F0FFF

// Estrutura para snapshot de processo (Toolhelp)
typedef struct tagPROCESSENTRY32W {
    DWORD dwSize;
    DWORD cntUsage;
    DWORD th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD th32ModuleID;
    DWORD cntThreads;
    DWORD th32ParentProcessID;
    LONG pcPriClassBase;
    DWORD dwFlags;
    WCHAR szExeFile[260];
} PROCESSENTRY32W, *PPROCESSENTRY32W;

typedef HANDLE (WINAPI *CreateToolhelp32Snapshot_t)(DWORD, DWORD);
typedef BOOL (WINAPI *Process32FirstW_t)(HANDLE, PPROCESSENTRY32W);
typedef BOOL (WINAPI *Process32NextW_t)(HANDLE, PPROCESSENTRY32W);

// Injeta UMA DLL via CreateRemoteThread + LoadLibraryA
static BOOL inject_dll(DWORD pid, const char* dll_path, const char* dll_name) {
    printf("[+] Injetando %s (PID %lu)...\n", dll_name, (unsigned long)pid);

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS_X, FALSE, pid);
    if (!hProc) {
        fprintf(stderr, "  [-] OpenProcess falhou: %lu\n", GetLastError());
        return FALSE;
    }

    // Verifica se é 64-bit
    BOOL isWow64 = FALSE;
    if (IsWow64Process(hProc, &isWow64) && isWow64) {
        fprintf(stderr, "  [-] Processo é 32-bit (WOW64) - DLLs sao x64\n");
        CloseHandle(hProc);
        return FALSE;
    }

    // Caminho da DLL
    size_t len = strlen(dll_path) + 1;
    void* remote_mem = VirtualAllocEx(hProc, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_mem) {
        fprintf(stderr, "  [-] VirtualAllocEx falhou: %lu\n", GetLastError());
        CloseHandle(hProc);
        return FALSE;
    }

    if (!WriteProcessMemory(hProc, remote_mem, dll_path, len, NULL)) {
        fprintf(stderr, "  [-] WriteProcessMemory falhou: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }

    FARPROC pLoadLibraryA = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!pLoadLibraryA) {
        fprintf(stderr, "  [-] GetProcAddress LoadLibraryA falhou\n");
        VirtualFreeEx(hProc, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }

    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibraryA, remote_mem, 0, NULL);
    if (!hThread) {
        fprintf(stderr, "  [-] CreateRemoteThread falhou: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }

    WaitForSingleObject(hThread, 10000);
    DWORD exit_code = 0;
    GetExitCodeThread(hThread, &exit_code);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remote_mem, 0, MEM_RELEASE);
    CloseHandle(hProc);

    if (exit_code == 0) {
        fprintf(stderr, "  [-] LoadLibraryA retornou 0 - DLL nao carregou (anti-cheat?)\n");
        return FALSE;
    }

    printf("  [+] %s injetada com sucesso (HMODULE=0x%lX)\n", dll_name, exit_code);
    return TRUE;
}

// Aguarda processo aparecer (ou lança se nao estiver rodando)
static DWORD wait_or_launch_process(const char* exe_path, const char* args) {
    // Primeiro tenta achar processo ja rodando
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = {0};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                char name[260];
                WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, name, sizeof(name), NULL, NULL);
                if (_stricmp(name, "CabalMain.exe") == 0) {
                    DWORD pid = pe.th32ProcessID;
                    CloseHandle(hSnap);
                    printf("[+] CabalMain.exe ja rodando (PID %lu)\n", (unsigned long)pid);
                    return pid;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    // Nao achou - lança o jogo
    printf("[+] Lançando %s %s ...\n", exe_path, args ? args : "");

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};

    char cmdline[1024];
    snprintf(cmdline, sizeof(cmdline), "\"%s\" %s", exe_path, args ? args : "");

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "[-] CreateProcess falhou: %lu\n", GetLastError());
        return 0;
    }

    printf("[+] Processo criado (PID %lu) - SUSPENSO\n", (unsigned long)pi.dwProcessId);

    // Injeta as DLLs ANTES de resumir (anti-cheat nao travou ainda)
    // As injecoes sao feitas com o processo suspenso
    // NOTA: CreateRemoteThread em processo suspenso FUNCIONA - a thread roda quando resume

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return pi.dwProcessId;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr,
            "Uso: %s <game_exe> <game_args> <dll1> <dll2>\n"
            "Exemplo: %s \"C:\\\\ConnectGame\\\\cabal.exe\" husky "
            "\"D:\\\\projeto\\\\Connect Game\\\\bin\\\\connect_game.dll\" "
            "\"D:\\\\projeto\\\\CABAL_Login\\\\x64\\\\Release\\\\probe.dll\"\n",
            argv[0], argv[0]);
        return 1;
    }

    const char* game_exe = argv[1];
    const char* game_args = argv[2];
    const char* dll1 = argv[3];
    const char* dll2 = argv[4];

    // Verifica arquivos
    if (GetFileAttributesA(game_exe) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "[-] Game exe nao encontrado: %s\n", game_exe);
        return 1;
    }
    if (GetFileAttributesA(dll1) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "[-] DLL1 nao encontrada: %s\n", dll1);
        return 1;
    }
    if (GetFileAttributesA(dll2) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "[-] DLL2 nao encontrada: %s\n", dll2);
        return 1;
    }

    printf("=== CABAL_Login Injector Duplo ===\n");
    printf("Game:    %s %s\n", game_exe, game_args);
    printf("DLL 1:   %s\n", dll1);
    printf("DLL 2:   %s\n", dll2);
    printf("\n");

    // Aguarda ou lança o processo
    DWORD pid = wait_or_launch_process(game_exe, game_args);
    if (!pid) {
        fprintf(stderr, "[-] Falha ao obter PID\n");
        return 1;
    }

    // Injeta DLL 1 (connect_game.dll)
    if (!inject_dll(pid, dll1, "connect_game.dll")) {
        fprintf(stderr, "[-] Falha ao injetar connect_game.dll\n");
        return 1;
    }

    Sleep(500); // Pequena pausa entre injecoes

    // Injeta DLL 2 (probe.dll)
    if (!inject_dll(pid, dll2, "probe.dll")) {
        fprintf(stderr, "[-] Falha ao injetar probe.dll\n");
        return 1;
    }

    printf("\n[+] AMBAS as DLLs injetadas com sucesso!\n");
    printf("[+] Conecte o MCP ao PID %lu\n", (unsigned long)pid);
    printf("[+] Va na tela de selecao de servidor -> CLIQUE MERCURIO\n");
    printf("[+] Va na tela de personagem -> CLIQUE INICIAR\n");
    printf("[+] Feche o jogo -> probe.log em D:\\projeto\\CABAL_Login\\probe.log\n");

    return 0;
}