#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CABAL_probe.py — Injeta APENAS probe.dll no LAUNCH do CabalMain.exe
Espera o processo aparecer, injeta, mostra PID.
Uso (Admin): python CABAL_probe.py
"""

import ctypes
import ctypes.wintypes
import sys
import os
import time

k32 = ctypes.windll.kernel32
PROCESS_ALL_ACCESS = 0x1F0FFF
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
MEM_RELEASE = 0x8000
PAGE_READWRITE = 0x04
CREATE_SUSPENDED = 0x00000004
TH32CS_SNAPPROCESS = 0x00000002
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [('dwSize', ctypes.wintypes.DWORD), ('cntUsage', ctypes.wintypes.DWORD),
                ('th32ProcessID', ctypes.wintypes.DWORD), ('th32DefaultHeapID', ctypes.c_void_p),
                ('th32ModuleID', ctypes.wintypes.DWORD), ('cntThreads', ctypes.wintypes.DWORD),
                ('th32ParentProcessID', ctypes.wintypes.DWORD), ('pcPriClassBase', ctypes.c_long),
                ('dwFlags', ctypes.wintypes.DWORD), ('szExeFile', ctypes.wintypes.WCHAR * 260)]

class STARTUPINFOA(ctypes.Structure):
    _fields_ = [('cb', ctypes.wintypes.DWORD), ('lpReserved', ctypes.c_char_p),
                ('lpDesktop', ctypes.c_char_p), ('lpTitle', ctypes.c_char_p),
                ('dwX', ctypes.wintypes.DWORD), ('dwY', ctypes.wintypes.DWORD),
                ('dwXSize', ctypes.wintypes.DWORD), ('dwYSize', ctypes.wintypes.DWORD),
                ('dwXCountChars', ctypes.wintypes.DWORD), ('dwYCountChars', ctypes.wintypes.DWORD),
                ('dwFillAttribute', ctypes.wintypes.DWORD), ('dwFlags', ctypes.wintypes.DWORD),
                ('wShowWindow', ctypes.wintypes.WORD), ('cbReserved2', ctypes.wintypes.WORD),
                ('lpReserved2', ctypes.c_void_p), ('hStdInput', ctypes.c_void_p),
                ('hStdOutput', ctypes.c_void_p), ('hStdError', ctypes.c_void_p)]

class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [('hProcess', ctypes.c_void_p), ('hThread', ctypes.c_void_p),
                ('dwProcessId', ctypes.wintypes.DWORD), ('dwThreadId', ctypes.wintypes.DWORD)]

def find_cabalmain_pid():
    h_snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if h_snap == INVALID_HANDLE_VALUE: return None
    try:
        pe = PROCESSENTRY32W(); pe.dwSize = ctypes.sizeof(PROCESSENTRY32W)
        if k32.Process32FirstW(h_snap, ctypes.byref(pe)):
            while True:
                name = ctypes.wstring_at(pe.szExeFile)
                if name.lower() == 'cabalmain.exe': return pe.th32ProcessID
                if not k32.Process32NextW(h_snap, ctypes.byref(pe)): break
    finally: k32.CloseHandle(h_snap)
    return None

def launch_cabal_suspended(exe_path, args):
    si = STARTUPINFOA(); si.cb = ctypes.sizeof(STARTUPINFOA)
    pi = PROCESS_INFORMATION()
    cmdline = f'"{exe_path}" {args}'.encode('utf-8')
    if not k32.CreateProcessA(None, cmdline, None, None, False, CREATE_SUSPENDED, None, None, ctypes.byref(si), ctypes.byref(pi)):
        raise ctypes.WinError(k32.GetLastError())
    print(f'[+] Processo criado SUSPENSO (PID {pi.dwProcessId})')
    k32.CloseHandle(pi.hThread)
    return pi.dwProcessId, pi.hProcess

def inject_dll(pid, h_process, dll_path, dll_name):
    print(f'[+] Injetando {dll_name}...')
    is_wow64 = ctypes.c_bool()
    if not k32.IsWow64Process(h_process, ctypes.byref(is_wow64)): raise ctypes.WinError(k32.GetLastError())
    if is_wow64.value: raise RuntimeError('Processo 32-bit - DLL x64')
    dll_full = os.path.abspath(dll_path)
    print(f'    DLL: {dll_full}')
    dll_bytes = dll_full.encode('utf-8') + b'\x00'
    remote_mem = k32.VirtualAllocEx(h_process, None, len(dll_bytes), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
    if not remote_mem: raise ctypes.WinError(k32.GetLastError())
    try:
        written = ctypes.c_size_t(0)
        if not k32.WriteProcessMemory(h_process, remote_mem, dll_bytes, len(dll_bytes), ctypes.byref(written)):
            raise ctypes.WinError(k32.GetLastError())
        p_loadlib = k32.LoadLibraryA
        h_thread = k32.CreateRemoteThread(h_process, None, 0, ctypes.cast(p_loadlib, ctypes.c_void_p), remote_mem, 0, None)
        if not h_thread:
            err = k32.GetLastError(); print(f'  [-] CreateRemoteThread falhou: WinError {err}'); raise ctypes.WinError(err)
        try:
            k32.WaitForSingleObject(h_thread, 10000)
            exit_code = ctypes.wintypes.DWORD(0)
            k32.GetExitCodeThread(h_thread, ctypes.byref(exit_code))
            if exit_code.value == 0: raise RuntimeError('LoadLibraryA retornou 0')
            print(f'  [+] {dll_name} injetada (HMODULE=0x{exit_code.value:X})')
            return True
        finally: k32.CloseHandle(h_thread)
    finally: k32.VirtualFreeEx(h_process, remote_mem, 0, MEM_RELEASE)

PROBE_DLL = r'D:\projeto\CABAL_Login\x64\Release\probe.dll'
GAME_EXE = r'C:\ConnectGame\cabal.exe'
GAME_ARGS = 'husky'

if not os.path.exists(PROBE_DLL):
    print(f'[ERRO] probe.dll nao encontrada: {PROBE_DLL}')
    input('Pressione Enter para sair...')
    sys.exit(1)

print('=== CABAL_Login — Injeta probe.dll ===')
print(f'probe.dll: {PROBE_DLL}')
print()

pid = find_cabalmain_pid()
h_process = None

if pid:
    print(f'[+] CabalMain.exe ja rodando (PID {pid})')
    h_process = k32.OpenProcess(0x1F0FFF, False, pid)
    if not h_process:
        err = k32.GetLastError()
        print(f'[-] OpenProcess falhou: WinError {err}')
        if err == 5: print('    -> Precisa ADMIN')
        input('Pressione Enter para sair...')
        sys.exit(1)
else:
    print('[+] CabalMain.exe nao encontrado - AGUARDANDO processo aparecer...')
    print('[+] Abra o jogo normalmente - vou injetar probe.dll assim que abrir')
    while True:
        pid = find_cabalmain_pid()
        if pid:
            print(f'[+] CabalMain.exe detectado (PID {pid})')
            h_process = k32.OpenProcess(0x1F0FFF, False, pid)
            if h_process: break
            else: print(f'[-] OpenProcess falhou, tentando...')
        time.sleep(1)

try:
    if not inject_dll(pid, h_process, PROBE_DLL, 'probe.dll'): sys.exit(1)
    print('\n[+] probe.dll INJETADA com sucesso!')
    print(f'[+] PID: {pid}')
    print('[+] Conecte o MCP /connect-game-mcp a este PID')
    print('[+] Faca os cliques: Mercurio -> Canal -> Iniciar')
    print(f'[+] Feche o jogo -> probe.log em D:\\projeto\\CABAL_Login\\probe.log')
except Exception as e:
    print(f'[ERRO] {e}')
finally:
    if h_process: k32.CloseHandle(h_process)

input('\nPressione Enter para sair...')