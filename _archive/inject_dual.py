#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
inject_dual.py — Injector Duplo Python (estilo Cabal_Proxy)
Injeta connect_game.dll + probe.dll no LAUNCH do CabalMain.exe

Uso (como Admin):
    python inject_dual.py

Baseado no connect_game_inject.exe: CreateRemoteThread + LoadLibraryA
Aguarda/lança o processo, injeta AS DUAS DLLs sequencialmente.
"""

import ctypes
import ctypes.wintypes
import sys
import os
import time

# ─── Constantes Win32 ───
PROCESS_ALL_ACCESS = 0x1F0FFF
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
MEM_RELEASE = 0x8000
PAGE_READWRITE = 0x04
CREATE_SUSPENDED = 0x00000004
TH32CS_SNAPPROCESS = 0x00000002
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

# ─── Estruturas ───
class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", ctypes.wintypes.DWORD),
        ("cntUsage", ctypes.wintypes.DWORD),
        ("th32ProcessID", ctypes.wintypes.DWORD),
        ("th32DefaultHeapID", ctypes.c_void_p),
        ("th32ModuleID", ctypes.wintypes.DWORD),
        ("cntThreads", ctypes.wintypes.DWORD),
        ("th32ParentProcessID", ctypes.wintypes.DWORD),
        ("pcPriClassBase", ctypes.c_long),
        ("dwFlags", ctypes.wintypes.DWORD),
        ("szExeFile", ctypes.wintypes.WCHAR * 260),
    ]

class STARTUPINFOA(ctypes.Structure):
    _fields_ = [
        ("cb", ctypes.wintypes.DWORD),
        ("lpReserved", ctypes.c_char_p),
        ("lpDesktop", ctypes.c_char_p),
        ("lpTitle", ctypes.c_char_p),
        ("dwX", ctypes.wintypes.DWORD),
        ("dwY", ctypes.wintypes.DWORD),
        ("dwXSize", ctypes.wintypes.DWORD),
        ("dwYSize", ctypes.wintypes.DWORD),
        ("dwXCountChars", ctypes.wintypes.DWORD),
        ("dwYCountChars", ctypes.wintypes.DWORD),
        ("dwFillAttribute", ctypes.wintypes.DWORD),
        ("dwFlags", ctypes.wintypes.DWORD),
        ("wShowWindow", ctypes.wintypes.WORD),
        ("cbReserved2", ctypes.wintypes.WORD),
        ("lpReserved2", ctypes.c_void_p),
        ("hStdInput", ctypes.c_void_p),
        ("hStdOutput", ctypes.c_void_p),
        ("hStdError", ctypes.c_void_p),
    ]

class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("hProcess", ctypes.c_void_p),
        ("hThread", ctypes.c_void_p),
        ("dwProcessId", ctypes.wintypes.DWORD),
        ("dwThreadId", ctypes.wintypes.DWORD),
    ]

# ─── DLLs kernel32 ───
k32 = ctypes.windll.kernel32

# ─── Helpers ───
def find_cabalmain_pid():
    """Procura CabalMain.exe já rodando via Toolhelp snapshot"""
    h_snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if h_snap == INVALID_HANDLE_VALUE:
        return None
    try:
        pe = PROCESSENTRY32W()
        pe.dwSize = ctypes.sizeof(PROCESSENTRY32W)
        if k32.Process32FirstW(h_snap, ctypes.byref(pe)):
            while True:
                name = ctypes.wstring_at(pe.szExeFile)
                if name.lower() == "cabalmain.exe":
                    return pe.th32ProcessID
                if not k32.Process32NextW(h_snap, ctypes.byref(pe)):
                    break
    finally:
        k32.CloseHandle(h_snap)
    return None

def launch_cabal_suspended(exe_path, args):
    """Cria processo SUSPENSO (anti-cheat ainda não inicializou)"""
    si = STARTUPINFOA()
    si.cb = ctypes.sizeof(STARTUPINFOA)
    pi = PROCESS_INFORMATION()

    cmdline = f'"{exe_path}" {args}'.encode('utf-8')

    if not k32.CreateProcessA(
        None, cmdline, None, None, False,
        CREATE_SUSPENDED, None, None,
        ctypes.byref(si), ctypes.byref(pi)
    ):
        raise ctypes.WinError(k32.GetLastError())

    print(f"[+] Processo criado SUSPENSO (PID {pi.dwProcessId})")

    # Fecha handles que não precisamos (o processo principal continua suspenso)
    k32.CloseHandle(pi.hThread)

    return pi.dwProcessId, pi.hProcess

def inject_dll(pid, h_process, dll_path, dll_name):
    """Injeta UMA DLL via CreateRemoteThread + LoadLibraryA"""
    print(f"[+] Injetando {dll_name}...")

    # Verifica se é 64-bit
    is_wow64 = ctypes.c_bool()
    if not k32.IsWow64Process(h_process, ctypes.byref(is_wow64)):
        raise ctypes.WinError(k32.GetLastError())
    if is_wow64.value:
        raise RuntimeError(f"Processo é 32-bit (WOW64) - DLLs são x64")

    # Caminho absoluto da DLL
    dll_full = os.path.abspath(dll_path)
    print(f"    DLL path: {dll_full}")
    dll_bytes = dll_full.encode('utf-8') + b'\x00'
    remote_mem = k32.VirtualAllocEx(
        h_process, None, len(dll_bytes),
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE
    )
    if not remote_mem:
        raise ctypes.WinError(k32.GetLastError())

    try:
        # Escreve o caminho
        written = ctypes.c_size_t(0)
        if not k32.WriteProcessMemory(h_process, remote_mem, dll_bytes, len(dll_bytes), ctypes.byref(written)):
            raise ctypes.WinError(k32.GetLastError())

        # LoadLibraryA do NOSSO kernel32.dll — mesmo endereço no processo alvo (base compartilhada)
        p_loadlib = k32.LoadLibraryA
        if not p_loadlib:
            raise RuntimeError("LoadLibraryA não encontrado no kernel32 local")

        # Cria thread remota
        h_thread = k32.CreateRemoteThread(
            h_process, None, 0,
            ctypes.cast(p_loadlib, ctypes.c_void_p),
            remote_mem, 0, None
        )
        if not h_thread:
            err = k32.GetLastError()
            print(f"  [-] CreateRemoteThread falhou: WinError {err}")
            raise ctypes.WinError(err)

        try:
            # Aguarda thread terminar
            k32.WaitForSingleObject(h_thread, 10000)
            exit_code = ctypes.wintypes.DWORD(0)
            k32.GetExitCodeThread(h_thread, ctypes.byref(exit_code))

            if exit_code.value == 0:
                raise RuntimeError(f"LoadLibraryA retornou 0 - DLL não carregou (anti-cheat?)")

            print(f"  [+] {dll_name} injetada (HMODULE=0x{exit_code.value:X})")
            return True
        finally:
            k32.CloseHandle(h_thread)
    finally:
        k32.VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE)

def main():
    # ─── Config (ajuste se necessário) ───
    PROJECT_DIR = r"D:\projeto\CABAL_Login"
    CONNECT_DIR = r"D:\projeto\Connect Game\bin"

    CONNECT_DLL = os.path.join(CONNECT_DIR, "connect_game.dll")
    PROBE_DLL = os.path.join(PROJECT_DIR, "x64", "Release", "probe.dll")

    # ─── Verificações das DLLs (obrigatórias) ───
    for path, name in [(CONNECT_DLL, "connect_game.dll"), (PROBE_DLL, "probe.dll")]:
        if not os.path.exists(path):
            print(f"[ERRO] {name} não encontrado: {path}")
            return 1

    print("=== CABAL_Login Injector Duplo (Python) ===")
    print(f"DLL 1:   {CONNECT_DLL}")
    print(f"DLL 2:   {PROBE_DLL}")
    print()

    # ─── 1. Procura CabalMain.exe já rodando ───
    pid = find_cabalmain_pid()
    h_process = None
    launched = False

    if pid:
        print(f"[+] CabalMain.exe já rodando (PID {pid})")
        # PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ
        h_process = k32.OpenProcess(0x1F0FFF, False, pid)
        if not h_process:
            err = k32.GetLastError()
            print(f"[-] OpenProcess falhou: WinError {err}")
            if err == 5:
                print("    -> Precisa rodar como ADMINISTRADOR")
            return 1
    else:
        print("[+] CabalMain.exe não encontrado - aguardando processo aparecer...")
        print(f"[+] Inicie o jogo manualmente (launcher/atalho) - vou injetar assim que abrir")

        # Modo WAIT: loop até o processo aparecer
        while True:
            pid = find_cabalmain_pid()
            if pid:
                print(f"[+] CabalMain.exe detectado (PID {pid})")
                h_process = k32.OpenProcess(0x1F0FFF, False, pid)
                if h_process:
                    break
                else:
                    err = k32.GetLastError()
                    print(f"[-] OpenProcess falhou (WinError {err}), tentando novamente...")
            time.sleep(1)

    try:
        # ─── 2. Injeta connect_game.dll ───
        if not inject_dll(pid, h_process, CONNECT_DLL, "connect_game.dll"):
            return 1

        time.sleep(0.5)  # Pausa entre injeções

        # ─── 3. Injeta probe.dll ───
        if not inject_dll(pid, h_process, PROBE_DLL, "probe.dll"):
            return 1

        # ─── 4. Retoma processo principal (se lançou suspenso) ───
        # Nota: se o processo já estava rodando, não retomamos
        # O CreateRemoteThread já executa no contexto do processo alvo

        print("\n[+] AMBAS as DLLs injetadas com sucesso!")
        print(f"[+] Conecte o MCP ao PID {pid}")
        print("[+] Va na tela de selecao de servidor -> CLIQUE MERCURIO")
        print("[+] Va na tela de personagem -> CLIQUE INICIAR")
        print(f"[+] Feche o jogo -> probe.log em {PROJECT_DIR}\\probe.log")

    finally:
        if h_process:
            k32.CloseHandle(h_process)

    return 0

if __name__ == "__main__":
    # Verifica admin
    try:
        is_admin = ctypes.windll.shell32.IsUserAnAdmin()
    except:
        is_admin = False

    if not is_admin:
        print("[AVISO] Recomendado rodar como Administrador!")
        print("   (Botão direito no terminal -> Executar como administrador)")
        print()

    sys.exit(main())