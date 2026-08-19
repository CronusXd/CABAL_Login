#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
login.py — CABAL_Login em PYTHON PURO (sem DLL, sem injeção, sem C).

Faz o login automático das contas do arquivo 'Cabal BR SUB.txt' no cliente
CabalMain.exe usando SOMENTE input de sistema (SendInput): cliques em
coordenadas + teclado. Nada é injetado — o anti-cheat não vê nenhum módulo
estranho no processo; o script apenas "opera o mouse e o teclado".

A janela do jogo PRECISA estar em primeiro plano (foco) durante a execução.
NÃO clique fora do jogo enquanto o script digita.

Uso:
    python login.py --check        # valida config + contas (sem enviar input)
    python login.py map            # modo de mapeamento dos botoes (F9 grava)
    python login.py run [--go]     # fluxo completo; aguarda F8 (no jogo) p/ iniciar
                                   #   --go : ja começa sem esperar F8
                                   #   --wait: aguarda CabalMain.exe aparecer

Comandos no jogo:
    F8            inicia o fluxo (quando pedir "aperte F8 no jogo")
    F9            no modo 'map': grava a posicao do mouse como o botao atual

Config: login.cfg (coordenadas dos botoes + tempos de espera). Veja
login.cfg.example para a lista completa.
"""

import ctypes
import ctypes.wintypes as wt
import os
import sys
import threading
import time
import queue
import datetime

try:
    import tkinter as tk
    HAS_TK = True
except ImportError:
    HAS_TK = False

try:
    import cv2
    import numpy as np
    HAS_CV2 = True
except ImportError:
    HAS_CV2 = False

try:
    import mss
    import mss.tools
    HAS_MSS = True
except ImportError:
    HAS_MSS = False

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# ─────────────────────────────────────────────────────────────────────────────
# Constantes Windows
# ─────────────────────────────────────────────────────────────────────────────
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
CFG_PATH = os.path.join(PROJECT_DIR, "login.cfg")
ACCOUNTS_PATH = os.path.join(PROJECT_DIR, "Cabal BR SUB.txt")
OUT_PATH = os.path.join(PROJECT_DIR, "output.txt")
CUR_PATH = os.path.join(PROJECT_DIR, "login.current")
_game_hwnd = None  # preenchido em run_flow, usado por click/type pra manter foco
N_PATH = os.path.join(PROJECT_DIR, "login.n")
STOP_PATH = os.path.join(PROJECT_DIR, "login.stop")
GO_PATH = os.path.join(PROJECT_DIR, "login.go")

# Input
INPUT_MOUSE = 0
INPUT_KEYBOARD = 1
KEYEVENTF_EXTENDEDKEY = 0x0001
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_UNICODE = 0x0004
KEYEVENTF_SCANCODE = 0x0008
MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_ABSOLUTE = 0x8000

# VKs
VK_TAB = 0x09
VK_RETURN = 0x0D
VK_BACK = 0x08
VK_ESCAPE = 0x1B
VK_CONTROL = 0x11
VK_F8 = 0x77
VK_F9 = 0x78
VK_F10 = 0x79
VK_I = 0x49
VK_O = 0x4F

TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPMODULE = 0x00000008
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400

# ─────────────────────────────────────────────────────────────────────────────
# Win32 bindings (ctypes)
# ─────────────────────────────────────────────────────────────────────────────
user32 = ctypes.windll.user32
k32 = ctypes.windll.kernel32


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [("dx", wt.LONG), ("dy", wt.LONG), ("mouseData", wt.DWORD),
                ("dwFlags", wt.DWORD), ("time", wt.DWORD), ("dwExtraInfo", ctypes.POINTER(wt.ULONG))]


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD),
                ("time", wt.DWORD), ("dwExtraInfo", ctypes.POINTER(wt.ULONG))]


class INPUTUNION(ctypes.Union):
    _fields_ = [("mi", MOUSEINPUT), ("ki", KEYBDINPUT)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", wt.DWORD), ("u", INPUTUNION)]


SendInput = user32.SendInput
SendInput.restype = wt.UINT
SendInput.argtypes = [wt.UINT, ctypes.POINTER(INPUT), ctypes.c_int]

GetAsyncKeyState = user32.GetAsyncKeyState
GetAsyncKeyState.restype = wt.SHORT
GetSystemMetrics = user32.GetSystemMetrics
GetCursorPos = user32.GetCursorPos


class POINT(ctypes.Structure):
    _fields_ = [("x", wt.LONG), ("y", wt.LONG)]


def _send_inputs(*inputs):
    """Envia uma lista de INPUT. Retorna True se todos foram enviados."""
    n = len(inputs)
    arr = (INPUT * n)(*inputs)
    sent = SendInput(n, arr, ctypes.sizeof(INPUT))
    return sent == n


def _key_inp(vk, scan, flags):
    i = INPUT(type=INPUT_KEYBOARD)
    i.u.ki.wVk = vk
    i.u.ki.wScan = scan
    i.u.ki.dwFlags = flags
    return i


def press_key(vk, scan=0):
    """Tecla down+up. Usa SCANCODE p/ Enter/Tab (DirectInput do jogo le scan)."""
    flags = 0
    if scan:
        flags = KEYEVENTF_SCANCODE
    else:
        scan = 0
    _send_inputs(_key_inp(vk, scan, flags), _key_inp(vk, scan, flags | KEYEVENTF_KEYUP))


def press_enter():
    press_key(VK_RETURN, 0x1C)   # scancode 0x1C = Enter


def press_tab():
    press_key(VK_TAB, 0x0F)      # scancode 0x0F = Tab


def press_ctrl_i():
    """Ctrl+I via scancode (CABAL le DirectInput, precisa de scan)."""
    key_combo(VK_CONTROL, ord('I'), ctrl_scan=0x1D, key_scan=0x17)


def press_escape():
    """Escape via scancode (DirectInput)."""
    press_key(VK_ESCAPE, 0x01)


def press_o():
    """Tecla O via scancode (DirectInput)."""
    press_key(ord('O'), 0x18)


def open_menu(hwnd):
    """Abre o menu do jogo (Esc para fechar qualquer menu aberto, depois O).
       Retorna True se abriu com sucesso."""
    focus_game(hwnd)
    # Fecha qualquer menu/janela aberta
    press_escape()
    time.sleep(0.5)
    # Abre o menu
    focus_game(hwnd)
    press_o()
    time.sleep(1.0)
    # Verifica que o jogo ainda tem foco
    return _ensure_focus()


def key_combo(ctrl_vk, key_vk, ctrl_scan=0, key_scan=0):
    """Ctrl+<tecla> via SENDINPUT (funciona em DirectInput).
       Se scancodes fornecidos, usa KEYEVENTF_SCANCODE."""
    flags_down = 0
    flags_up = KEYEVENTF_KEYUP
    if ctrl_scan and key_scan:
        flags_down = KEYEVENTF_SCANCODE
        flags_up = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP
    else:
        ctrl_scan = 0
        key_scan = 0
    _send_inputs(_key_inp(ctrl_vk, ctrl_scan, flags_down),
                 _key_inp(key_vk, key_scan, flags_down))
    time.sleep(0.04)
    _send_inputs(_key_inp(key_vk, key_scan, flags_up),
                 _key_inp(ctrl_vk, ctrl_scan, flags_up))
    time.sleep(0.08)


def clear_field():
    """Seleciona tudo e apaga (Ctrl+A + Backspace)."""
    key_combo(VK_CONTROL, ord('A'), ctrl_scan=0x1D, key_scan=0x1E)
    press_key(VK_BACK)
    time.sleep(0.1)


def _ensure_focus():
    """Garante que o jogo esta em primeiro plano. Retorna True se focou."""
    if not _game_hwnd:
        return False
    ok = focus_game(_game_hwnd)
    if not ok:
        log("    WARN: focus_game falhou — jogo pode nao estar em primeiro plano")
    return ok


def type_text(s, delay=0.03):
    """Digita texto via KEYEVENTF_UNICODE (funciona com qualquer layout)."""
    if not s:
        return
    _ensure_focus()
    for ch in s:
        _send_inputs(
            _key_inp(0, ord(ch), KEYEVENTF_UNICODE),
            _key_inp(0, ord(ch), KEYEVENTF_UNICODE | KEYEVENTF_KEYUP),
        )
        if delay > 0:
            time.sleep(delay)


def click(x, y):
    """Clique do mouse em (x,y) EM PIXELS da tela (coords ABSOLUTAS do jogo)."""
    _ensure_focus()
    sw = GetSystemMetrics(0)   # SM_CXSCREEN
    sh = GetSystemMetrics(1)   # SM_CYSCREEN
    if sw <= 0 or sh <= 0:
        return False
    dx = int((x * 65535) / sw)
    dy = int((y * 65535) / sh)
    m = INPUT(type=INPUT_MOUSE)
    m.u.mi.dx = dx
    m.u.mi.dy = dy
    m.u.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE
    down = INPUT(type=INPUT_MOUSE)
    down.u.mi.dwFlags = MOUSEEVENTF_LEFTDOWN
    up = INPUT(type=INPUT_MOUSE)
    up.u.mi.dwFlags = MOUSEEVENTF_LEFTUP
    _send_inputs(m, down, up)
    _update_gui()
    time.sleep(0.3)
    return True


def _update_gui():
    """No-op — _pump_logs() na main thread atualiza a GUI via queue."""


# ─────────────────────────────────────────────────────────────────────────────
# Janela / processo do jogo
# ─────────────────────────────────────────────────────────────────────────────
class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
                ("th32ProcessID", wt.DWORD), ("th32DefaultHeapID", ctypes.c_void_p),
                ("th32ModuleID", wt.DWORD), ("cntThreads", wt.DWORD),
                ("th32ParentProcessID", wt.DWORD), ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", wt.DWORD), ("szExeFile", wt.WCHAR * 260)]


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("th32ModuleID", wt.DWORD),
                ("th32ProcessID", wt.DWORD), ("GlblcntUsage", wt.DWORD),
                ("ProccntUsage", wt.DWORD), ("modBaseAddr", ctypes.POINTER(wt.BYTE)),
                ("modBaseSize", wt.DWORD), ("hModule", wt.HMODULE),
                ("szModule", wt.WCHAR * 256), ("szExePath", wt.WCHAR * 260)]


INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value


def find_game_pid():
    """PID do CabalMain.exe via Toolhelp (None se nao rodando)."""
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snap == INVALID_HANDLE_VALUE:
        return None
    try:
        pe = PROCESSENTRY32W()
        pe.dwSize = ctypes.sizeof(PROCESSENTRY32W)
        ok = k32.Process32FirstW(snap, ctypes.byref(pe))
        while ok:
            if pe.szExeFile.lower() == "cabalmain.exe":
                return pe.th32ProcessID
            ok = k32.Process32NextW(snap, ctypes.byref(pe))
    finally:
        k32.CloseHandle(snap)
    return None


WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


def find_game_hwnd(pid):
    """HWND da janela principal do jogo (visivel e com titulo), None se nao achar."""
    found = []

    @WNDENUMPROC
    def cb(hwnd, _lparam):
        if not user32.IsWindowVisible(hwnd):
            return True
        wpid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wpid))
        if wpid.value == pid:
            length = user32.GetWindowTextLengthW(hwnd)
            if length > 0:
                buf = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buf, length + 1)
                if buf.value:
                    found.append(hwnd)
                    return False   # primeira janela com titulo = principal
        return True

    user32.EnumWindows(cb, 0)
    return found[0] if found else None


def focus_game(hwnd):
    """Maximiza e coloca a janela do jogo em primeiro plano (agressivo, multi-tentativa)."""
    user32.ShowWindow(hwnd, 3)   # SW_MAXIMIZE
    # Tentativa 1: SetForegroundWindow direto
    for _ in range(3):
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.15)
        if user32.GetForegroundWindow() == hwnd:
            return True
    # Tentativa 2: ALT key trick (libera o bloqueio de foreground do Windows)
    for _ in range(3):
        _send_inputs(_key_inp(0x12, 0, 0), _key_inp(0x12, 0, KEYEVENTF_KEYUP))
        time.sleep(0.1)
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.2)
        if user32.GetForegroundWindow() == hwnd:
            return True
    # Tentativa 3: BringWindowToTop + SetFocus
    try:
        user32.BringWindowToTop(hwnd)
        time.sleep(0.1)
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.15)
    except Exception:
        pass
    return user32.GetForegroundWindow() == hwnd


def keep_focus(hwnd):
    """Checa se o jogo esta em primeiro plano; se nao, recoloca com retry."""
    try:
        if user32.GetForegroundWindow() == hwnd:
            return
        user32.ShowWindow(hwnd, 3)  # SW_MAXIMIZE
        # ALT trick para liberar foreground lock
        _send_inputs(_key_inp(0x12, 0, 0), _key_inp(0x12, 0, KEYEVENTF_KEYUP))
        time.sleep(0.1)
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.15)
    except Exception:
        pass


# ─────────────────────────────────────────────────────────────────────────────
# GUI — janela de log + botao START (tkinter, thread-safe)
# ─────────────────────────────────────────────────────────────────────────────
_log_queue = queue.Queue()
_log_text = None
_log_root = None
_log_status = None
_log_start_btn = None
_log_stop_btn = None
_flow_running = False
_run_wait = True
_stop_requested = False


def _append_log(msg):
    """Enfileira mensagem para exibir na GUI (thread-safe)."""
    _log_queue.put(msg)


def _pump_logs():
    """Processa fila de logs e atualiza o Text widget (chamado na main thread)."""
    if _log_text:
        count = 0
        while not _log_queue.empty() and count < 50:
            try:
                msg = _log_queue.get_nowait()
                _log_text.insert("end", msg + "\n")
                _log_text.see("end")
                count += 1
            except queue.Empty:
                break
    if _log_root:
        _log_root.after(50, _pump_logs)


def _update_status(text):
    """Atualiza barra de status (chamado na main thread)."""
    if _log_status:
        _log_status.config(text=text)


def setup_log_window():
    """Cria janela GUI: log + botoes START/STOP, canto inferior esquerdo, sempre por cima."""
    global _log_root, _log_text, _log_status, _log_start_btn, _log_stop_btn

    import tkinter as tk

    root = tk.Tk()
    root.title("CABAL Login")
    sh = GetSystemMetrics(1)
    root.geometry("500x320+5+{}".format(sh - 400))
    root.configure(bg="#1a1a2e")
    root.attributes("-topmost", True)
    root.protocol("WM_DELETE_WINDOW", lambda: None)

    # Topo: titulo + status
    top = tk.Frame(root, bg="#1a1a2e")
    top.pack(fill="x", padx=5, pady=(5, 0))
    tk.Label(top, text="CABAL Login", bg="#1a1a2e", fg="#00ff41",
             font=("Consolas", 10, "bold")).pack(side="left")
    _log_status = tk.Label(top, text="PARADO", bg="#1a1a2e", fg="#ff4444",
                           font=("Consolas", 9))
    _log_status.pack(side="right")

    # Bottom: botoes (pack ANTES do text pra sempre ficar visivel)
    bottom = tk.Frame(root, bg="#1a1a2e")
    bottom.pack(fill="x", padx=5, pady=(0, 5), side="bottom")

    def on_start():
        global _flow_running, _stop_requested
        if _flow_running:
            return
        _stop_requested = False
        _flow_running = True
        _log_start_btn.config(state="disabled", bg="#333")
        _log_stop_btn.config(state="normal", bg="#aa0000")
        _log_status.config(text="RODANDO...", fg="#00ff41")
        t = threading.Thread(target=_run_flow_thread, daemon=True)
        t.start()

    def on_stop():
        global _stop_requested
        _stop_requested = True
        _log_status.config(text="PARANDO...", fg="#ffaa00")
        _append_log("** STOP solicitado — aguardando fluxo parar **")

    _log_start_btn = tk.Button(bottom, text="  START  ", command=on_start,
                               bg="#00aa00", fg="white",
                               font=("Consolas", 11, "bold"),
                               relief="flat", cursor="hand2")
    _log_start_btn.pack(side="left", padx=(0, 10))

    _log_stop_btn = tk.Button(bottom, text="  STOP  ", command=on_stop,
                              bg="#555", fg="white",
                              font=("Consolas", 11, "bold"),
                              relief="flat", cursor="hand2", state="disabled")
    _log_stop_btn.pack(side="left", padx=(0, 10))

    tk.Button(bottom, text="Limpar", command=lambda: _log_text.delete("1.0", "end"),
              bg="#333", fg="#aaa", font=("Consolas", 9),
              relief="flat").pack(side="left")

    # Area de log (pack DEPOIS do bottom — preenche o espaco restante)
    frame = tk.Frame(root, bg="#0f0f23")
    frame.pack(fill="both", expand=True, padx=5, pady=5)
    text = tk.Text(frame, bg="#0f0f23", fg="#00ff41",
                   font=("Consolas", 9), wrap="word",
                   insertbackground="#00ff41", relief="flat",
                   state="normal")
    scroll = tk.Scrollbar(frame, command=text.yview)
    text.configure(yscrollcommand=scroll.set)
    scroll.pack(side="right", fill="y")
    text.pack(side="left", fill="both", expand=True)
    _log_text = text

    _log_root = root
    _append_log("CABAL Login — clique START para iniciar")
    _pump_logs()

    log("Janela GUI: 720x450 canto inferior esquerdo")
    root.mainloop()


def _run_flow_thread():
    """Executa o fluxo em thread separada (nao bloqueia a GUI)."""
    global _flow_running
    try:
        run_flow(go_immediately=True, wait_process=_run_wait)
    except Exception as e:
        _append_log("ERRO FLOW: %s" % e)
    finally:
        _flow_running = False
        # Restaura botoes na main thread
        if _log_root:
            def _reset_btns():
                _log_start_btn.config(state="normal", bg="#00aa00")
                _log_stop_btn.config(state="disabled", bg="#555")
                _log_status.config(text="FINALIZADO", fg="#ffaa00")
            _log_root.after(0, _reset_btns)
        _append_log("** Fluxo finalizado **")


# ─────────────────────────────────────────────────────────────────────────────
# Overlay de logs (tkinter) — janela fixa no canto inferior esquerdo
# ─────────────────────────────────────────────────────────────────────────────


def wait_with_focus(hwnd, seconds, label=""):
    """Espera N segundos mantendo o jogo em primeiro plano a cada 0.5s.
       Retorna False se STOP foi solicitado (para o fluxo imediatamente)."""
    end = time.time() + seconds
    while time.time() < end:
        if _stop_requested:
            log("    ** STOP — interrompendo espera **")
            return False
        keep_focus(hwnd)
        time.sleep(0.5)
    if label:
        log("    ok %s" % label)
    return True


# ─────────────────────────────────────────────────────────────────────────────
# Config (login.cfg) — coordenadas dos botoes + tempos de espera
# ─────────────────────────────────────────────────────────────────────────────
COORD_KEYS = ["entrar", "servidor", "canal", "comeca",
              "selecionar_servidor", "sim", "desconectar"]

DEFAULTS = {
    # tempos (segundos) — o requisito: >= 2s entre acoes
    "wait_action": 2.0,     # pausa padrao depois de uma acao
    "wait_login": 6.0,      # apos Enter de login ate clicar no servidor
    "wait_server": 4.0,     # apos clicar Venus ate selecionar canal
    "wait_char": 5.0,       # apos confirmar canal ate clicar ENTRAR
    "wait_sub": 2.5,        # janela de subsenha abrir
    "wait_world": 5.0,      # apos Comeca+subsenha ate carregar o mundo
    "wait_back": 15.0,      # tempo p/ voltar da desconexao pra tela de login
    "double_enter": 1,      # Enter duplo na tela de login (padrao: sim)
    "logout": 1,            # 1=voltar pro login e continuar; 0=parar apos 1a conta
    "verify": 0,            # 1=ler ouro/nivel/char da memoria (ReadProcessMemory)
    "ok_x": 1224,           # coordenada X do botao OK na tela de subsenha
    "ok_y": 638,            # coordenada Y do botao OK na tela de subsenha
    "grid_x": 1090,         # coordenada X inicial do grid de digitos da subsenha
    "grid_y": 470,          # coordenada Y inicial do grid de digitos da subsenha
    "cell_w": 50,           # largura de cada celula do teclado numerico
    "cell_h": 40,           # altura de cada celula do teclado numerico
    "grid_cols": 3,         # colunas do grid de digitos
    "grid_rows": 4,         # linhas do grid de digitos (0-9 + extras)
    "cell_gap": 4,          # espacamento entre celulas (pixels)
}

coords = {}      # nome -> (x, y)
opts = dict(DEFAULTS)   # scalars


def parse_cfg():
    """Le login.cfg: linhas 'nome x y' (botao) ou 'nome valor' (opcao)."""
    coords.clear()
    opts.update(DEFAULTS)
    path = CFG_PATH
    if not os.path.exists(path):
        return
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            name = parts[0].lower()
            if name in COORD_KEYS:
                try:
                    coords[name] = (int(parts[1]), int(parts[2]))
                except (ValueError, IndexError):
                    pass
            elif name in DEFAULTS:
                try:
                    if isinstance(DEFAULTS[name], int):
                        opts[name] = int(parts[1])
                    else:
                        opts[name] = float(parts[1])
                except ValueError:
                    pass


def write_cfg():
    """Regrava login.cfg (usado no modo map) preservando as opcoes."""
    lines = ["# login.cfg — coords dos botoes (mapeados com F9) + opcoes de tempo.",
             "# Formato: 'nome x y' (botao) ou 'nome valor' (opcao).\n"]
    for k in COORD_KEYS:
        x, y = coords.get(k, (0, 0))
        lines.append("%s %d %d" % (k, x, y))
    lines.append("")
    for k, v in opts.items():
        lines.append("%s %s" % (k, ("%g" % v) if isinstance(v, float) else str(v)))
    with open(CFG_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


# ─────────────────────────────────────────────────────────────────────────────
# Contas (Cabal BR SUB.txt — formato "Conta N / Usuario: / Senha: / subsenha: / NV:")
# ─────────────────────────────────────────────────────────────────────────────
def parse_accounts():
    accs = []
    if not os.path.exists(ACCOUNTS_PATH):
        return accs
    with open(ACCOUNTS_PATH, "r", encoding="utf-8", errors="replace") as f:
        cur = None
        for raw in f:
            line = raw.rstrip("\r\n").strip()
            if not line:
                continue
            if line.startswith("Conta"):
                if cur and cur.get("user") and cur.get("pass"):
                    accs.append(cur)
                cur = {}
                continue
            if ":" not in line:
                continue
            key, _, val = line.partition(":")
            val = val.strip()
            k = key.strip().lower()
            if k.startswith("usuario"):
                cur["user"] = val
            elif k.startswith("senha"):
                cur["pass"] = val
            elif k.startswith("subsenha"):
                cur["subsenha"] = val
            elif k.strip().upper() == "NV":
                try:
                    cur["nivel"] = int(val)
                except ValueError:
                    cur["nivel"] = 0
        if cur and cur.get("user") and cur.get("pass"):
            accs.append(cur)
    return accs


# ─────────────────────────────────────────────────────────────────────────────
# Logging (console + arquivo login_<idx>.log)
# ─────────────────────────────────────────────────────────────────────────────
g_logfile = None


def log(msg, end="\n"):
    ts = time.strftime("%H:%M:%S")
    line = "[%s] %s" % (ts, msg)
    print(line, end=end, flush=True)
    _append_log(line)
    if g_logfile:
        g_logfile.write(line + end)
        g_logfile.flush()


# ─────────────────────────────────────────────────────────────────────────────
# Indices de progresso (login.current / login.n / login.stop / done_*)
# ─────────────────────────────────────────────────────────────────────────────
def read_int_file(path, default):
    try:
        with open(path, "r") as f:
            return int(f.read().strip())
    except (IOError, ValueError):
        return default


def stop_requested():
    return os.path.exists(STOP_PATH)


def mark_done(idx):
    with open(os.path.join(PROJECT_DIR, "done_%d" % idx), "w") as f:
        f.write("")


def read_last_index_from_output():
    """Le o output.txt e retorna o ultimo numero de conta processado (ou -1)."""
    if not os.path.exists(OUT_PATH):
        return -1
    last_idx = -1
    try:
        with open(OUT_PATH, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.strip()
                if line.startswith("Conta "):
                    try:
                        last_idx = int(line.split()[1])
                    except (ValueError, IndexError):
                        pass
    except IOError:
        pass
    return last_idx


def write_output_line(acc, idx, status):
    """Escreve resultado da conta no output.txt no formato:
    Conta X
    Usuario: ...
    Senha: ...
    subsenha: ...
    Status: ...
    """
    block = (
        "Conta %d\n"
        "Usuario: %s\n"
        "Senha: %s\n"
        "subsenha: %s\n"
        "Status: %s\n"
    ) % (idx, acc.get("user", "?"), acc.get("pass", "?"),
         acc.get("subsenha", "?"), status)
    with open(OUT_PATH, "a", encoding="utf-8") as f:
        f.write(block + "\n")


# ─────────────────────────────────────────────────────────────────────────────
# Verificacao opcional (le ouro/nivel/char via ReadProcessMemory — fora do jogo)
# ─────────────────────────────────────────────────────────────────────────────
# Offsets VALIDADOS do build EP33 (legado): ouro+0x137F170, nome+0x137F190,
# nivel+0x137F1B8. RVA fixo do binario — igual ao que a DLL lia internamente.
def module_base(pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid)
    if snap == INVALID_HANDLE_VALUE:
        return None
    try:
        me = MODULEENTRY32W()
        me.dwSize = ctypes.sizeof(MODULEENTRY32W)
        if k32.Module32FirstW(snap, ctypes.byref(me)):
            # me.modBaseAddr é POINTER(BYTE) — o valor do ponteiro = endereço base no processo remoto
            return int(me.modBaseAddr)
    finally:
        k32.CloseHandle(snap)
    return None


def read_mem(pid, addr, size):
    h = k32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
    if not h:
        return b""
    try:
        buf = ctypes.create_string_buffer(size)
        read = ctypes.c_size_t(0)
        ok = k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, size, ctypes.byref(read))
        return buf.raw[:read.value] if ok else b""
    finally:
        k32.CloseHandle(h)


def _cstr(raw):
    return raw.split(b"\x00", 1)[0].decode("utf-8", errors="replace")


def verify_account(pid, acc):
    """Le ouro/nivel/nome do jogo. Retorna string para output.txt ou '' se falhar."""
    try:
        base = module_base(pid)
        if not base:
            return ""
        gold = int.from_bytes(read_mem(pid, base + 0x137F170, 4), "little")
        name = _cstr(read_mem(pid, base + 0x137F190, 20))
        lvl = int.from_bytes(read_mem(pid, base + 0x137F1B8, 4), "little")
        if not name and gold == 0 and lvl <= 0:
            return "verificacao=VAZIA"
        return "char=%s;nivel=%s;ouro=%s;src=python" % (name or "?", lvl or "?", gold)
    except Exception:
        return "verificacao=FALHOU"


# ─────────────────────────────────────────────────────────────────────────────
# Acões do fluxo (cada uma termina com a pausa >= wait_action entre acoes)
# ─────────────────────────────────────────────────────────────────────────────
def need_coord(name):
    return name in coords and coords[name] != (0, 0)


def action_delay():
    time.sleep(max(2.0, opts["wait_action"]))   # garantia: >= 2s


# ─────────────────────────────────────────────────────────────────────────────
# Deteccao de subsenha por reconhecimento de imagens (mss + Pillow)
# ─────────────────────────────────────────────────────────────────────────────
IMG_DIR = os.path.join(PROJECT_DIR, "imagens")
_digit_refs = {}   # cache: digito (int) -> PIL.Image


def _load_digit_refs():
    """Carrega imagens de referencia 0-9.jpg de imagens/ e cacheia."""
    global _digit_refs
    if _digit_refs:
        return _digit_refs
    for d in range(10):
        path = os.path.join(IMG_DIR, "%d.jpg" % d)
        if os.path.exists(path):
            try:
                _digit_refs[d] = Image.open(path).convert("RGB")
            except Exception as e:
                log("    WARN: erro ao carregar %s: %s" % (path, e))
        else:
            log("    WARN: %s nao encontrada (digito %d indisponivel)" % (path, d))
    return _digit_refs


def _game_region(hwnd):
    """Retorna (left, top, right, bottom) em coords da tela para a janela do jogo."""
    try:
        r = wt.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(r))
        return (r.left, r.top, r.right, r.bottom)
    except Exception:
        return (0, 0, 1920, 1080)


def _screenshot_game(hwnd, region=None):
    """Screenshot da janela do jogo. region=(x1,y1,x2,y2) relativo a janela (None=tudo).
       Retorna PIL.Image em modo RGB."""
    if not HAS_MSS or not HAS_PIL:
        return None
    left, top, right, bottom = _game_region(hwnd)
    if region:
        x1, y1, x2, y2 = region
        left += x1
        top += y1
        right = left + (x2 - x1)
        bottom = top + (y2 - y1)
    with mss.MSS() as sct:
        shot = sct.grab({"left": left, "top": top,
                         "width": right - left, "height": bottom - top})
    return Image.frombytes("RGB", shot.size, shot.bgra, "raw", "BGRX")


def detect_subsenha_digits(hwnd, subsenha="", quiet=False):
    """Usa cv2.matchTemplate para encontrar cada digito da subsenha na tela."""
    import cv2
    import numpy as np

    refs = _load_digit_refs()
    if not refs:
        return []

    shot = _screenshot_game(hwnd)
    if shot is None:
        return []

    # Converte screenshot para numpy (OpenCV)
    screen = np.array(shot)
    screen_bgr = cv2.cvtColor(screen, cv2.COLOR_RGB2BGR)

    wleft, wtop = _game_region(hwnd)[:2]
    ok_x, ok_y = opts["ok_x"], opts["ok_y"]

    # Regiao do dialogo de subsenha (acima do OK)
    rx1 = max(0, ok_x - wleft - 280)
    ry1 = max(0, ok_y - wtop - 280)
    rx2 = min(screen_bgr.shape[1], ok_x - wleft + 150)
    ry2 = min(screen_bgr.shape[0], ok_y - wtop)
    roi = screen_bgr[ry1:ry2, rx1:rx2]

    if not quiet:
        log("    Buscando subsenha '%s'" % subsenha)
    results = []
    MIN_MATCH = 0.95  # 95% minimo para subsenha (digitos)

    for ch in subsenha:
        if not ch.isdigit():
            continue
        d = int(ch)
        if d not in refs:
            log("    %d: sem referencia" % d)
            continue

        # Carrega referencia e converte para BGR
        ref_img = np.array(refs[d].convert("RGB"))
        ref_bgr = cv2.cvtColor(ref_img, cv2.COLOR_RGB2BGR)
        rh, rw = ref_bgr.shape[:2]

        # Template matching
        result = cv2.matchTemplate(roi, ref_bgr, cv2.TM_CCOEFF_NORMED)
        _, max_val, _, max_loc = cv2.minMaxLoc(result)

        # Coordenada absoluta na tela
        ax = wleft + rx1 + max_loc[0] + rw // 2
        ay = wtop + ry1 + max_loc[1] + rh // 2
        if max_val >= MIN_MATCH:
            log("    %d => (%d,%d) match=%.3f OK" % (d, ax, ay, max_val))
            results.append((d, ax, ay))

    if not results and not quiet:
        log("    Nenhum digito com match bom — subsenha nao necessaria")
    return results


# ─────────────────────────────────────────────────────────────────────────────
# Deteccao de imagens genericas (Selecionar Servidor, sim, Venus, Mercury)
# ─────────────────────────────────────────────────────────────────────────────
_img_cache = {}  # nome -> PIL.Image


def _load_img_ref(name):
    """Carrega imagem de referencia de imagens/<name>.jpg e cacheia."""
    if name in _img_cache:
        return _img_cache[name]
    path = os.path.join(IMG_DIR, "%s.jpg" % name)
    if not os.path.exists(path):
        log("    WARN: %s nao encontrada" % path)
        return None
    try:
        img = Image.open(path).convert("RGB")
        _img_cache[name] = img
        return img
    except Exception as e:
        log("    WARN: erro ao carregar %s: %s" % (path, e))
        return None


def find_on_screen(hwnd, img_name, threshold=0.90):
    """Procura imagem na tela do jogo. Retorna (x, y) em coords da tela ou None."""
    if not HAS_CV2 or not HAS_MSS or not HAS_PIL:
        return None
    ref = _load_img_ref(img_name)
    if ref is None:
        return None
    shot = _screenshot_game(hwnd)
    if shot is None:
        return None
    screen = np.array(shot)
    screen_bgr = cv2.cvtColor(screen, cv2.COLOR_RGB2BGR)
    ref_arr = np.array(ref.convert("RGB"))
    ref_bgr = cv2.cvtColor(ref_arr, cv2.COLOR_RGB2BGR)
    result = cv2.matchTemplate(screen_bgr, ref_bgr, cv2.TM_CCOEFF_NORMED)
    _, max_val, _, max_loc = cv2.minMaxLoc(result)
    if max_val >= threshold:
        rh, rw = ref_bgr.shape[:2]
        left, top = _game_region(hwnd)[:2]
        ax = left + max_loc[0] + rw // 2
        ay = top + max_loc[1] + rh // 2
        log("    find_on_screen('%s') => (%d,%d) match=%.3f" % (img_name, ax, ay, max_val))
        return (ax, ay)
    log("    find_on_screen('%s') => NAO ENCONTRADO (match=%.3f < %.3f)" %
        (img_name, max_val, threshold))
    return None


def click_image(hwnd, img_name, wait=2.0, threshold=0.90):
    """Procura imagem na tela, clica nela. Retorna True se encontrou e clicou."""
    pos = find_on_screen(hwnd, img_name, threshold)
    if pos:
        click(pos[0], pos[1])
        if wait > 0:
            wait_with_focus(hwnd, wait)
        return True
    return False


def find_on_screen_multi(hwnd, img_names, threshold=0.90):
    """Tenta encontrar qualquer uma das imagens na tela.
    Retorna (x, y, nome_encontrado) ou (None, None, None)."""
    for name in img_names:
        pos = find_on_screen(hwnd, name, threshold=threshold)
        if pos:
            return pos[0], pos[1], name
    return None, None, None


def click_image_multi(hwnd, img_names, wait=2.0, threshold=0.90):
    """Tenta encontrar e clicar em qualquer uma das imagens. Retorna nome encontrado ou None."""
    x, y, name = find_on_screen_multi(hwnd, img_names, threshold=threshold)
    if name:
        click(x, y)
        if wait > 0:
            wait_with_focus(hwnd, wait)
        return name
    return None


def move_mouse_center(hwnd):
    """Move mouse para o centro da tela do jogo (evita hover em labels)."""
    left, top, right, bottom = _game_region(hwnd)
    cx = (left + right) // 2
    cy = (top + bottom) // 2
    sw = GetSystemMetrics(0)  # SM_CXSCREEN
    sh = GetSystemMetrics(1)  # SM_CYSCREEN
    if sw <= 0 or sh <= 0:
        return
    dx = int((cx * 65535) / sw)
    dy = int((cy * 65535) / sh)
    m = INPUT(type=INPUT_MOUSE)
    m.u.mi.dx = dx
    m.u.mi.dy = dy
    m.u.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE
    _send_inputs(m)
    time.sleep(0.3)


def _verify_server_visibility(hwnd, target_server, threshold=0.90):
    """Verifica se as imagens de servidor/canal estao corretas.
    - target_server='venus': SOMENTE venus visivel, ZERO Mercury na tela
    - target_server='Mercury': Mercury visivel + Canal_venus NAO visivel
    Retorna True se condicoes atendidas."""
    mercury_variants = ["Mercury", "Mercury2"]
    if target_server.lower() == "venus":
        # Venus: SOMENTE venus visivel, ZERO Mercury na tela
        venus_pos = find_on_screen_multi(hwnd, ["venus", "venus2"], threshold=threshold)
        mercury_pos = find_on_screen_multi(hwnd, mercury_variants, threshold=threshold)
        venus_ok = venus_pos[0] is not None
        mercury_ok = mercury_pos[0] is not None
        if venus_ok and not mercury_ok:
            return True
        if mercury_ok:
            log("    Mercury visivel quando deveria ser so Venus — refresh")
        if not venus_ok:
            log("    Venus nao visivel")
        return False
    else:
        # Mercury: pelo menos 1 Mercury visivel + Canal_venus NAO pode estar visivel
        mercury_pos = find_on_screen_multi(hwnd, mercury_variants, threshold=threshold)
        canal_venus_pos = find_on_screen_multi(hwnd, ["Canal_venus"], threshold=threshold)
        mercury_ok = mercury_pos[0] is not None
        canal_venus_ok = canal_venus_pos[0] is not None
        if mercury_ok and not canal_venus_ok:
            return True
        if not mercury_ok:
            log("    Mercury nao visivel (nenhuma variante encontrada)")
        if canal_venus_ok:
            log("    Canal_venus visivel quando deveria ser so Canal_Mercury — refresh")
        return False


def take_world_screenshot(hwnd, acc_name, server_label):
    """Tira screenshot do mundo, salva em pasta com data atual.
       Formato: <data>/<acc_name>_<server_label>.png"""
    if not HAS_MSS or not HAS_PIL:
        log("    WARN: mss/PIL indisponivel — screenshot cancelada")
        return
    shot = _screenshot_game(hwnd)
    if shot is None:
        log("    WARN: falha ao capturar screenshot")
        return
    today = datetime.date.today().strftime("%Y-%m-%d")
    folder = os.path.join(PROJECT_DIR, today)
    os.makedirs(folder, exist_ok=True)
    fname = "%s_%s.png" % (acc_name, server_label)
    fpath = os.path.join(folder, fname)
    shot.save(fpath, "PNG")
    log("    => Screenshot salva: %s" % fpath)


def _clear_backspace(hwnd, times=20, delay=0.05):
    """Pressiona Backspace rapidamente para limpar campo."""
    focus_game(hwnd)
    for _ in range(times):
        press_key(VK_BACK)
        time.sleep(delay)


def do_credentials(acc, hwnd, idx=0):
    focus_game(hwnd)
    log("    => aguardando 2s apos START")
    time.sleep(2.0)
    log("    => click pre-login em (1379,597)")
    focus_game(hwnd)
    click(1379, 597)
    if not wait_with_focus(hwnd, 1.0):
        return False

    # Limpar campo de usuario com Backspace
    log("    => limpando campo usuario (20x Backspace)")
    _clear_backspace(hwnd, 20, 0.05)
    log("    => digitando usuario: %s" % acc["user"])
    type_text(acc["user"])

    press_tab()
    time.sleep(0.2)

    # Limpar campo de senha com Backspace
    log("    => limpando campo senha (20x Backspace)")
    _clear_backspace(hwnd, 20, 0.05)
    log("    => digitando senha")
    type_text(acc["pass"])
    time.sleep(0.5)
    press_enter()
    if opts["double_enter"]:
        time.sleep(0.4)
        press_enter()

    # Verificar login: aguardar 3s e procurar venus/venus2
    log("    => aguardando 3s para verificar login...")
    wait_with_focus(hwnd, 3.0)
    venus_pos = find_on_screen_multi(hwnd, ["venus", "venus2"], threshold=0.90)
    if venus_pos[0]:
        log("    => Login OK — Venus encontrado na tela")
        return True

    # Login falhou — screenshot + ESC
    log("    => FALHA NO LOGIN — Venus NAO encontrado")
    _do_screenshot_fail(hwnd, idx)
    log("    => Pressionando ESC")
    focus_game(hwnd)
    press_escape()
    time.sleep(1.0)
    log("    => Click OK em (1300,590)")
    focus_game(hwnd)
    click(1300, 590)
    time.sleep(1.0)
    return False


# ── Funções auxiliares do fluxo ──────────────────────────────────────────────
def _ask_manual_subsenha(sub):
    """Abre dialogo pedindo pra usuario digitar a subsenha manualmente.
    Retorna True se usuario confirmou, False se cancelou.
    Se tkinter indisponivel, aguarda 5s e prossegue."""
    if not HAS_TK:
        log("    tkinter indisponivel — aguardando 5s para usuario digitar no jogo")
        time.sleep(5.0)
        return True

    result = {"ok": False}

    def on_ok():
        result["ok"] = True
        dlg.destroy()

    def on_cancel():
        dlg.destroy()

    dlg = tk.Toplevel()
    dlg.title("Subsenha Manual")
    dlg.geometry("350x150")
    dlg.attributes("-topmost", True)
    dlg.resizable(False, False)

    tk.Label(dlg, text="Digitos da subsenha nao detectados automaticamente.",
             wraplength=320).pack(pady=(10, 5))
    tk.Label(dlg, text="Por favor, digite a subsenha no jogo\nclique OK quando terminar.",
             wraplength=320).pack(pady=(0, 10))

    btn_frame = tk.Frame(dlg)
    btn_frame.pack(pady=5)
    tk.Button(btn_frame, text="OK - Digitei!", command=on_ok, width=15,
              bg="#4CAF50", fg="white").pack(side="left", padx=5)
    tk.Button(btn_frame, text="Cancelar", command=on_cancel, width=15).pack(side="left", padx=5)

    dlg.update_idletasks()
    w = dlg.winfo_width()
    h = dlg.winfo_height()
    x = (dlg.winfo_screenwidth() // 2) - (w // 2)
    y = (dlg.winfo_screenheight() // 2) - (h // 2)
    dlg.geometry("+%d+%d" % (x, y))

    dlg.mainloop()
    return result["ok"]


def _do_subsenha(hwnd, sub):
    """Detecta e clica digitos da subsenha (95% minimo).
    Retry por 3s. Se nao detectar, presume que ja entrou no mundo."""
    if not (HAS_MSS and HAS_PIL and sub):
        if not sub:
            log("    => conta sem subsenha, continuando")
        return True
    focus_game(hwnd)
    deadline = time.time() + 3.0
    attempt = 0
    while time.time() < deadline:
        attempt += 1
        digits_on_screen = detect_subsenha_digits(hwnd, sub, quiet=True)
        if digits_on_screen:
            ok_x = opts["ok_x"]
            ok_y = opts["ok_y"]
            for i, (d, dx, dy) in enumerate(digits_on_screen):
                log("    => [%d/%d] clicando %d em (%d,%d)" %
                    (i + 1, len(digits_on_screen), d, dx, dy))
                focus_game(hwnd)
                click(dx, dy)
                if i < len(digits_on_screen) - 1:
                    time.sleep(0.5)
            log("    => OK em (%d,%d)" % (ok_x, ok_y))
            time.sleep(0.3)
            focus_game(hwnd)
            click(ok_x, ok_y)
            log("    Subsenha %s digitada com sucesso (auto)" % sub)
            if not wait_with_focus(hwnd, 2.0):
                return False
            return True
        time.sleep(0.5)
    # Nenhuma subsenha detectada apos 3s → entrou no mundo direto
    log("    Subsenha Sem necessidade — personagem ja entrou no mundo (scan %d tentativas)" % attempt)
    return True


def _do_click_comeca(hwnd, sub):
    """Clica COMECA + subsenha (procura por 3s). Retorna True se ok."""
    if need_coord("comeca"):
        x, y = coords["comeca"]
        log("    Click COMECA em (%d,%d)" % (x, y))
        focus_game(hwnd)
        click(x, y)
        log("    => aguardando 3s para subsenha aparecer")
        if not wait_with_focus(hwnd, 3.0):
            return False
        if not _do_subsenha(hwnd, sub):
            return False
    else:
        log("    ERRO: 'comeca' nao mapeado")
        return False
    return True


def _do_screenshot(hwnd, idx, server_label):
    """Ctrl+I + screenshot do inventario."""
    log("    Ctrl+I + screenshot inventario %s" % server_label)
    if not focus_game(hwnd):
        log("    WARN: jogo nao esta em primeiro plano — tentando mesmo assim")
    press_ctrl_i()
    time.sleep(1.0)
    take_world_screenshot(hwnd, "Conta%02d" % idx, "%s_inventario" % server_label)


def _do_screenshot_char(hwnd, idx, server_label):
    """Screenshot da tela de selecao de personagem (sem Ctrl+I)."""
    log("    Screenshot char %s" % server_label)
    if not focus_game(hwnd):
        log("    WARN: jogo nao esta em primeiro plano — tentando mesmo assim")
    time.sleep(1.0)
    take_world_screenshot(hwnd, "Conta%02d" % idx, "%s_char" % server_label)


def _do_screenshot_fail(hwnd, idx):
    """Screenshot de falha de login — salva como ContaXX_Fail.png."""
    today = datetime.date.today().strftime("%Y-%m-%d")
    folder = os.path.join(PROJECT_DIR, today)
    os.makedirs(folder, exist_ok=True)
    fname = "Conta%02d_Fail.png" % idx
    fpath = os.path.join(folder, fname)
    shot = _screenshot_game(hwnd)
    if shot is None:
        log("    WARN: falha ao capturar screenshot de falha")
        return
    shot.save(fpath, "PNG")
    log("    => Screenshot falha salva: %s" % fpath)


def _do_select_channel(hwnd, server_image, canal_image, canal_label, max_retries=15):
    """Seleciona canal por imagem.
    Fluxo: mouse centro → procurar canal → verificar servidor → clicar.
    Venus: SOMENTE venus visivel, ZERO Mercury.
    Mercury: Mercury visivel + Canal_venus NAO visivel."""
    canal_thresh = 0.90
    srv_thresh = 0.90
    server_variants = [server_image, server_image + "2"]
    other_server = "venus" if "venus" not in server_image.lower() else "Mercury"
    other_variants = [other_server, other_server + "2"]
    for attempt in range(max_retries):
        if _stop_requested:
            return False
        # 1) Mouse pro centro — MOMENTO de procurar o canal
        move_mouse_center(hwnd)
        focus_game(hwnd)
        pos = find_on_screen(hwnd, canal_image, threshold=canal_thresh)
        if pos:
            # 2) Canal encontrado — verificar servidor ANTES de clicar
            focus_game(hwnd)
            if not _verify_server_visibility(hwnd, server_image, threshold=srv_thresh):
                log("    %s encontrado mas servidor incorreto (tentativa %d) — refresh" %
                    (canal_label, attempt + 1))
                focus_game(hwnd)
                clicked = click_image_multi(hwnd, server_variants, wait=1.0)
                if not clicked:
                    click_image_multi(hwnd, other_variants, wait=1.0)
                if not wait_with_focus(hwnd, 1.5):
                    return False
                continue
            # 3) Servidor OK — clicar no canal
            log("    %s encontrado em %s (tentativa %d)" % (canal_label, server_image, attempt + 1))
            focus_game(hwnd)
            click(pos[0], pos[1])
            if not wait_with_focus(hwnd, 2.0):
                return False
            return True
        # Canal nao encontrado — refresh
        log("    %s nao encontrado (match<%.0f%%), refresh (tentativa %d/%d)" %
            (canal_label, canal_thresh * 100, attempt + 1, max_retries))
        focus_game(hwnd)
        clicked = click_image_multi(hwnd, server_variants, wait=1.0)
        if not clicked:
            click_image_multi(hwnd, other_variants, wait=1.0)
        if not wait_with_focus(hwnd, 1.0):
            return False
    log("    ERRO: %s nao encontrado apos %d tentativas" % (canal_label, max_retries))
    return False


def _do_mercury_selection(hwnd):
    """Navega O → Selecionar Servidor → sim → Venus → Mercury → canal → ENTRAR."""
    log("    Press O -> Selecionar Servidor")
    if not open_menu(hwnd):
        log("    WARN: foco pode ter sido perdido")
    if not wait_with_focus(hwnd, 2.0):
        return False

    found = False
    for attempt in range(3):
        if _stop_requested:
            return False
        focus_game(hwnd)
        if click_image(hwnd, "Selecionar Servidor", wait=1.0, threshold=0.55):
            found = True
            break
        log("    Tentativa %d/3 — Selecionar Servidor nao encontrado, retry..." % (attempt + 1))
        time.sleep(1.0)
        open_menu(hwnd)
        wait_with_focus(hwnd, 2.0)
    if not found:
        log("    ERRO: 'Selecionar Servidor' nao encontrado apos 3 tentativas")
        return False

    log("    Click sim")
    if not click_image(hwnd, "sim", wait=1.0, threshold=0.55):
        log("    ERRO: 'sim' nao encontrado")
        return False

    if _stop_requested:
        return False

    log("    Click Venus")
    if not click_image(hwnd, "venus", wait=1.0):
        log("    ERRO: 'venus' nao encontrado")
        return False

    log("    Click Mercury")
    if not click_image(hwnd, "Mercury", wait=1.0):
        log("    ERRO: 'Mercury' nao encontrado")
        return False

    # Selecionar canal Mercury por imagem
    if not _do_select_channel(hwnd, "Mercury", "Canal_Mercury", "Canal Mercury"):
        return False

    log("    Click ENTRAR em (2431,989)")
    focus_game(hwnd)
    click(2431, 989)
    if not wait_with_focus(hwnd, 2.0):
        return False

    return True


def _do_open_server_menu(hwnd):
    """Navega O → 1s → Selecionar Servidor → 1s → sim → 9s.
    Retorna True se chegou na tela de selecao de servidor."""
    log("    Press O")
    if not open_menu(hwnd):
        log("    WARN: foco pode ter sido perdido")
    if not wait_with_focus(hwnd, 1.0):
        return False

    log("    Procurando Selecionar Servidor")
    found = False
    for attempt in range(3):
        if _stop_requested:
            return False
        focus_game(hwnd)
        if click_image(hwnd, "Selecionar Servidor", wait=1.0, threshold=0.55):
            found = True
            break
        log("    Tentativa %d/3 — Selecionar Servidor nao encontrado, retry..." % (attempt + 1))
        time.sleep(1.0)
        open_menu(hwnd)
        wait_with_focus(hwnd, 1.0)
    if not found:
        log("    ERRO: 'Selecionar Servidor' nao encontrado apos 3 tentativas")
        return False

    log("    Click sim")
    if not click_image(hwnd, "sim", wait=1.0, threshold=0.55):
        log("    ERRO: 'sim' nao encontrado")
        return False

    log("    Aguardando 5s para canais carregarem")
    if not wait_with_focus(hwnd, 5.0):
        return False

    return True


def _do_mercury_channel_only(hwnd):
    """Navega O→Selecionar Servidor→sim, procura Canal_Mercury.
    Fluxo: encontrar Mercury/Mercury2 primeiro → procurar Canal_Mercury → clicar."""
    log("    O -> Selecionar Servidor -> sim (aguardar 5s)")
    if not _do_open_server_menu(hwnd):
        return False

    mercury_variants = ["Mercury", "Mercury2"]
    venus_variants = ["venus", "venus2"]
    canal_thresh = 0.60
    srv_thresh = 0.90

    # FASE 1: Garantir que Mercury/Mercury2 esta visivel
    for attempt in range(15):
        if _stop_requested:
            return False
        move_mouse_center(hwnd)
        focus_game(hwnd)
        srv_pos = find_on_screen_multi(hwnd, mercury_variants, threshold=srv_thresh)
        if srv_pos[0]:
            log("    Mercury/Mercury2 encontrado (tentativa %d)" % (attempt + 1))
            break
        log("    Mercury nao visivel, clicando Mercury pra refresh (tentativa %d/15)" % (attempt + 1))
        focus_game(hwnd)
        clicked = click_image_multi(hwnd, mercury_variants, wait=1.0)
        if not clicked:
            click_image_multi(hwnd, venus_variants, wait=1.0)
        if not wait_with_focus(hwnd, 1.5):
            return False
    else:
        log("    ERRO: Mercury nao encontrado apos 15 tentativas")
        return False

    # FASE 2: Procurar Canal_Mercury
    for attempt in range(15):
        if _stop_requested:
            return False
        move_mouse_center(hwnd)
        focus_game(hwnd)
        pos = find_on_screen(hwnd, "Canal_Mercury", threshold=canal_thresh)
        if pos:
            log("    Canal Mercury encontrado (tentativa %d)" % (attempt + 1))
            focus_game(hwnd)
            click(pos[0], pos[1])
            if not wait_with_focus(hwnd, 2.0):
                return False
            log("    Click ENTRAR em (2431,989)")
            focus_game(hwnd)
            click(2431, 989)
            if not wait_with_focus(hwnd, 2.0):
                return False
            return True
        log("    Canal Mercury nao encontrado (match<%.0f%%), refresh (tentativa %d/15)" %
            (canal_thresh * 100, attempt + 1))
        focus_game(hwnd)
        clicked = click_image_multi(hwnd, mercury_variants, wait=1.0)
        if not clicked:
            click_image_multi(hwnd, venus_variants, wait=1.0)
        if not wait_with_focus(hwnd, 1.5):
            return False
    log("    ERRO: Canal Mercury nao encontrado apos 15 tentativas")
    return False


def _do_disconnect(hwnd):
    """Navega O → 1s → Selecionar Servidor → 1s → sim → 9s → Desconectar."""
    log("    Press O")
    if not open_menu(hwnd):
        log("    WARN: foco pode ter sido perdido")
    if not wait_with_focus(hwnd, 1.0):
        return False

    log("    Procurando Selecionar Servidor")
    found = False
    for attempt in range(3):
        if _stop_requested:
            return False
        focus_game(hwnd)
        if click_image(hwnd, "Selecionar Servidor", wait=1.0, threshold=0.55):
            found = True
            break
        log("    Tentativa %d/3 — Selecionar Servidor nao encontrado, retry..." % (attempt + 1))
        time.sleep(1.0)
        open_menu(hwnd)
        wait_with_focus(hwnd, 1.0)
    if not found:
        log("    ERRO: 'Selecionar Servidor' nao encontrado apos 3 tentativas")
        return False

    log("    Click sim")
    if not click_image(hwnd, "sim", wait=1.0, threshold=0.55):
        log("    ERRO: 'sim' nao encontrado")
        return False

    log("    Aguardando 5s para tela de Desconectar")
    if not wait_with_focus(hwnd, 5.0):
        return False

    log("    Click Desconectar em (2315,988)")
    focus_game(hwnd)
    click(2315, 988)
    if not wait_with_focus(hwnd, 2.0):
        return False

    return True


def flow_run_account(acc, idx, hwnd, pid):
    """Roda uma conta completa com verificacao de nivel (personagem).
    Se Venus sem nivel -> Tela Anterior -> redirect Mercury.
    Se Mercury sem nivel -> Tela Anterior -> Desconectar."""
    user = acc["user"]
    sub = acc.get("subsenha", "")
    log("========== CONTA %d: %s ==========" % (idx, user))

    # ═══ FASE 1: Login no Venus ═══
    log("--- FASE 1: Login Venus ---")
    focus_game(hwnd)
    if not do_credentials(acc, hwnd, idx):
        log("    Login falhou — pulando conta %d" % idx)
        return False

    if _stop_requested:
        return False

    # Selecionar canal Venus por imagem
    if not _do_select_channel(hwnd, "venus", "Canal_venus", "Canal Venus"):
        return False

    if _stop_requested:
        return False

    # Click ENTRAR
    if need_coord("entrar"):
        x, y = coords["entrar"]
        log("3) Click ENTRAR em (%d,%d)" % (x, y))
        focus_game(hwnd)
        click(x, y)
        if not wait_with_focus(hwnd, 2.0):
            return False
    else:
        log("3) ERRO: 'entrar' nao mapeado")
        return False

    if _stop_requested:
        return False

    # ═══ Verificar nivel.jpg (personagem existe?) ═══
    log("4) Verificando nivel.jpg (personagem no Venus)...")
    nivel_pos = find_on_screen(hwnd, "nivel", threshold=0.90)

    if nivel_pos:
        # Venus tem personagem → aguardar 1s + screenshot char + COMECA
        log("    nivel.jpg encontrado — aguardando 1s + screenshot char + COMECA")
        log("    => aguardando 1s apos nivel.jpg")
        if not wait_with_focus(hwnd, 1.0):
            return False
        _do_screenshot_char(hwnd, idx, "Venus")
        log("    => aguardando 1s apos screenshot char")
        if not wait_with_focus(hwnd, 1.0):
            return False
        if not _do_click_comeca(hwnd, sub):
            return False

        if _stop_requested:
            return False

        # Aguarda mundo carregar (15s)
        log("5) Aguardando mundo Venus carregar (%.1fs)" % opts["wait_world"])
        if not wait_with_focus(hwnd, opts["wait_world"]):
            return False

        if _stop_requested:
            return False

        # Screenshot Venus
        log("6) Screenshot Venus")
        _do_screenshot(hwnd, idx, "Venus")

        if _stop_requested:
            return False

        # ═══ FASE 2: Trocar para Mercury ═══
        log("--- FASE 2: Trocar para Mercury ---")
        if not _do_mercury_channel_only(hwnd):
            return False

        if _stop_requested:
            return False

        # ═══ Verificar nivel.jpg em Mercury ═══
        log("13) Verificando nivel.jpg (personagem no Mercury)...")
        nivel_pos_m = find_on_screen(hwnd, "nivel", threshold=0.90)

        if nivel_pos_m:
            # Mercury tem personagem → aguardar 1s + screenshot char + COMECA
            log("    nivel.jpg encontrado — aguardando 1s + screenshot char + COMECA")
            log("    => aguardando 1s apos nivel.jpg")
            if not wait_with_focus(hwnd, 1.0):
                return False
            _do_screenshot_char(hwnd, idx, "Mercury")
            log("    => aguardando 1s apos screenshot char")
            if not wait_with_focus(hwnd, 1.0):
                return False
            if not _do_click_comeca(hwnd, sub):
                return False

            if _stop_requested:
                return False

            # Aguarda mundo Mercury carregar
            log("14) Aguardando mundo Mercury carregar (%.1fs)" % opts["wait_world"])
            if not wait_with_focus(hwnd, opts["wait_world"]):
                return False

            if _stop_requested:
                return False

            # Screenshot Mercury
            log("15) Screenshot Mercury")
            _do_screenshot(hwnd, idx, "Mercury")
        else:
            # Mercury sem personagem → Tela Anterior + Desconectar
            log("    nivel.jpg NAO encontrado no Mercury — Tela Anterior + Desconectar")
            log("    Click Tela Anterior em (2310,990)")
            focus_game(hwnd)
            click(2310, 990)
            if not wait_with_focus(hwnd, 2.0):
                return False
            log("--- FASE 3: Desconectar ---")
            log("    Click Desconectar em (2315,988)")
            focus_game(hwnd)
            click(2315, 988)
            if not wait_with_focus(hwnd, 2.0):
                return False
            write_output_line(acc, idx, "Logada com sucesso")
            log("    *** CONTA %d FINALIZADA (Mercury sem nivel) ***" % idx)
            return True

    else:
        # Venus sem personagem → Tela Anterior → redirecionar para Mercury
        log("    nivel.jpg NAO encontrado no Venus — redirecionando para Mercury")
        log("    Click Tela Anterior em (2310,990)")
        focus_game(hwnd)
        click(2310, 990)
        if not wait_with_focus(hwnd, 2.0):
            return False

        # ═══ FASE 2 (via redirect): Mercury direto ═══
        log("--- FASE 2: Redirect para Mercury (Venus sem nivel) ---")
        if not _do_mercury_channel_only(hwnd):
            return False

        if _stop_requested:
            return False

        # ═══ Verificar nivel.jpg em Mercury ═══
        log("13) Verificando nivel.jpg (personagem no Mercury)...")
        nivel_pos_m = find_on_screen(hwnd, "nivel", threshold=0.90)

        if nivel_pos_m:
            log("    nivel encontrado no Mercury — aguardando 1s + screenshot char + COMECA")
            log("    => aguardando 1s apos nivel.jpg")
            if not wait_with_focus(hwnd, 1.0):
                return False
            _do_screenshot_char(hwnd, idx, "Mercury")
            log("    => aguardando 1s apos screenshot char")
            if not wait_with_focus(hwnd, 1.0):
                return False
            if not _do_click_comeca(hwnd, sub):
                return False

            if _stop_requested:
                return False

            log("14) Aguardando mundo Mercury carregar (%.1fs)" % opts["wait_world"])
            if not wait_with_focus(hwnd, opts["wait_world"]):
                return False

            if _stop_requested:
                return False

            log("15) Screenshot Mercury")
            _do_screenshot(hwnd, idx, "Mercury")
        else:
            # Mercury sem personagem → Tela Anterior + Desconectar
            log("    nivel.jpg NAO encontrado no Mercury — Tela Anterior + Desconectar")
            log("    Click Tela Anterior em (2310,990)")
            focus_game(hwnd)
            click(2310, 990)
            if not wait_with_focus(hwnd, 2.0):
                return False
            log("--- FASE 3: Desconectar ---")
            log("    Click Desconectar em (2315,988)")
            focus_game(hwnd)
            click(2315, 988)
            if not wait_with_focus(hwnd, 2.0):
                return False
            write_output_line(acc, idx, "Logada com sucesso")
            log("    *** CONTA %d FINALIZADA (Mercury sem nivel) ***" % idx)
            return True

    # ═══ FASE 3: Desconectar ═══
    log("--- FASE 3: Desconectar ---")
    if not _do_disconnect(hwnd):
        return False

    write_output_line(acc, idx, "Logada com sucesso")
    log("    *** CONTA %d FINALIZADA ***" % idx)
    return True


# ─────────────────────────────────────────────────────────────────────────────
# Modo de mapeamento (F9 grava a posicao do mouse para cada botao)
# ─────────────────────────────────────────────────────────────────────────────
MAP_LABELS = COORD_KEYS


def map_mode():
    parse_cfg()
    print("=== MODO DE MAPEAMENTO (login.py map) ===")
    print("Passe o MOUSE sobre cada botao e aperte F9 na ordem:")
    print("  " + " -> ".join(MAP_LABELS))
    print("F10 cancela. O jogo deve estar na tela correspondente a cada botao.\n")
    mapped = 0
    for name in MAP_LABELS:
        print("[MAP] (%d/%d) F9 sobre: %s" % (mapped + 1, len(MAP_LABELS), name))
        wait_key(VK_F9, VK_F10)
        pt = POINT()
        GetCursorPos(ctypes.byref(pt))
        coords[name] = (int(pt.x), int(pt.y))
        print("[MAP] %s = (%d,%d)" % (name, pt.x, pt.y))
        mapped += 1
        time.sleep(0.2)
    write_cfg()
    print("[MAP] pronto — %d botao(s) gravado(s) em login.cfg (mantendo as opcoes)" % mapped)


def wait_key(want, cancel=0):
    """Aguarda a tecla `want` (transicao) ou `cancel` p/ abortar. Retorna True se `want`."""
    last = False
    while True:
        now = bool(GetAsyncKeyState(want) & 1)
        if now and not last:      # borda de descida do pulso
            time.sleep(0.15)
            return True
        if cancel and (GetAsyncKeyState(cancel) & 1):
            print("[MAP] cancelado.")
            sys.exit(0)
        last = now
        time.sleep(0.05)


# ─────────────────────────────────────────────────────────────────────────────
# Fluxo principal (loop de contas)
# ─────────────────────────────────────────────────────────────────────────────
def run_flow(go_immediately, wait_process):
    parse_cfg()

    if wait_process:
        log("Aguardando CabalMain.exe aparecer (rodar 'python inject...' nao e mais necessario)...")
        pid = None
        while not pid:
            pid = find_game_pid()
            if pid:
                break
            time.sleep(1)
    else:
        pid = find_game_pid()
        if not pid:
            log("ERRO: CabalMain.exe nao esta rodando. Abra o jogo e rode 'python login.py run --wait'")
            return 1
    log("CabalMain.exe: PID %d" % pid)

    hwnd = find_game_hwnd(pid)
    if not hwnd:
        log("ERRO: nao achei a janela principal do jogo (PID %d)" % pid)
        return 1
    global _game_hwnd
    _game_hwnd = hwnd
    log("Janela do jogo encontrada (hwnd=0x%X). Nao clique fora do jogo durante a execucao." % hwnd)
    focus_game(hwnd)

    accs = parse_accounts()
    if not accs:
        log("ERRO: nenhuma conta lida de '%s'" % ACCOUNTS_PATH)
        return 1
    log("Contas carregadas: %d" % len(accs))

    last_done = read_last_index_from_output()
    idx = last_done + 1 if last_done >= 0 else 0
    if idx >= len(accs):
        log("Todas as %d contas ja foram processadas (ultimo: %d)" % (len(accs), last_done))
        return 0
    if idx < 0:
        idx = 0
    limit = read_int_file(N_PATH, len(accs))
    if limit <= 0 or limit > len(accs):
        limit = len(accs)
    if idx > 0:
        log("Retomando da conta %d (ultimo processado: %d)" % (idx, last_done))
    log("Processando %d conta(s): indices %d..%d" % (limit - idx, idx, limit - 1))

    # gatilho F8 (no jogo) exceto com --go
    if not go_immediately:
        log(">>> FOCO NO JOGO e aperte F8 para iniciar (ou crie 'login.go' na raiz)")
        print(">>> Aguardando F8 ou login.go ...", flush=True)
        ok = False
        while not ok:
            if GetAsyncKeyState(VK_F8) & 0x8000:
                while GetAsyncKeyState(VK_F8) & 0x8000:
                    time.sleep(0.05)
                ok = True
            elif os.path.exists(GO_PATH):
                os.remove(GO_PATH)
                ok = True
            time.sleep(0.1)
        log("F8 detectado — iniciando fluxo")

    idx_start = idx
    while idx < limit:
        _update_gui()
        acc = accs[idx]
        logfile_path = os.path.join(PROJECT_DIR, "login_%d.log" % idx)
        global g_logfile
        g_logfile = open(logfile_path, "w", encoding="utf-8", buffering=1)
        log("# conta %d/%d (%s) — log: %s" % (idx, limit, acc["user"], logfile_path))

        if stop_requested() or _stop_requested:
            log("login.stop presente — parando.")
            g_logfile.close()
            break

        try:
            entered = flow_run_account(acc, idx, hwnd, pid)
        except Exception as e:      # nunca deixar uma conta derrubar o loop
            log("EXCECAO na conta %d: %r — continuando na proxima" % (idx, e))
            entered = False

        g_logfile.close()
        g_logfile = None

        if entered:
            mark_done(idx)
            log("conta %d finalizada (entrou no mundo)" % idx)
            if opts["logout"] and need_coord("selecionar_servidor") and need_coord("desconectar"):
                pass  # fluxo de disconnect ja feito dentro de flow_run_account
            idx += 1
        else:
            log("conta %d falhou — proxima conta em 2s" % idx)
            write_output_line(acc, idx, "Fail")
            idx += 1

        log("Aguardando 2s antes da proxima conta...")
        time.sleep(2.0)

    if g_logfile:
        g_logfile.close()

    with open(os.path.join(PROJECT_DIR, "done_all"), "w") as f:
        f.write("")
    log("done_all criado — loop concluido (sem fechar o jogo)")
    return 0


# ─────────────────────────────────────────────────────────────────────────────
# Modo --check: valida config + contas sem enviar nenhum input
# ─────────────────────────────────────────────────────────────────────────────
def check_mode():
    parse_cfg()
    print("=== CHECK (sem enviar nada) ===")
    print("login.cfg:" if os.path.exists(CFG_PATH) else "login.cfg: (nao existe — usando defaults + coords vazias)")
    for k in COORD_KEYS:
        print("  %-18s %s" % (k, coords.get(k, "(nao configurado)")))
    print("opcoes:")
    for k, v in opts.items():
        print("  %-14s %s" % (k, _fmt(v)))
    accs = parse_accounts()
    print("contas: %d (arquivo: %s)" % (len(accs), os.path.basename(ACCOUNTS_PATH)))
    for i, a in enumerate(accs[:3]):
        print("  [%d] user=%s pass=*** subsenha=%s nivel=%s" %
              (i, a.get("user"), a.get("subsenha", ""), a.get("nivel", "?")))
    if len(accs) > 3:
        print("  ... (%d a mais)" % (len(accs) - 3))
    pid = find_game_pid()
    print("CabalMain.exe: %s" % ("PID %d" % pid if pid else "NAO rodando"))
    if pid:
        hwnd = find_game_hwnd(pid)
        print("  janela: %s" % ("0x%X" % hwnd if hwnd else "nao encontrada"))

    # valida o minimo p/ o fluxo (fase atual: so canal + entrar)
    missing = [k for k in ("canal", "entrar") if not need_coord(k)]
    print()
    if missing:
        print("! coords obrigatorias faltando: %s — rode 'python login.py map'" % ", ".join(missing))
    else:
        print("coords obrigatorias OK (canal + entrar)")
    if opts["logout"] and (not need_coord("selecionar_servidor") or not need_coord("desconectar")):
        print("! logout ligado mas 'selecionar_servidor'/'desconectar' nao mapeados - loop para apos a 1a conta")

    # Verifica refs de imagem
    if HAS_MSS and HAS_PIL:
        refs = _load_digit_refs()
        missing = [d for d in range(10) if d not in refs]
        if missing:
            print("! imagens de digitos faltando: %s (em %s)" % (missing, IMG_DIR))
        else:
            print("imagens de digitos: OK (0-9 em %s)" % IMG_DIR)
    else:
        deps = []
        if not HAS_MSS:
            deps.append("mss")
        if not HAS_PIL:
            deps.append("Pillow")
        print("! subsenha por imagem indisponivel — instale: pip install %s" % " ".join(deps))
    return 0


def _fmt(v):
    return ("%.1f" % v) if isinstance(v, float) else str(v)


# ─────────────────────────────────────────────────────────────────────────────
def main():
    args = sys.argv[1:]
    if "--check" in args or (args and args[0] in ("check", "chk")):
        return check_mode()
    if args and args[0] in ("map", "m"):
        return map_mode()

    # Modo run: sempre abre GUI
    # Salva flags para quando START for clicado
    global _run_wait
    _run_wait = "--wait" in args
    setup_log_window()
    return 0


if __name__ == "__main__":
    sys.exit(main())