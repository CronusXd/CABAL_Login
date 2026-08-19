"""test_confianca_subsenha.py — Verifica confiança dos dígitos da subsenha na tela atual."""
import sys
import os
import ctypes
from ctypes import wintypes
import time

# ── Carrega imagens de referência ──
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    import cv2
    import numpy as np
    import mss
    import mss.tools
    from PIL import Image
except ImportError as e:
    print("ERRO: pip install opencv-python mss Pillow numpy")
    sys.exit(1)

# ── Constantes ──
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
IMG_DIR = os.path.join(PROJECT_DIR, "imagens")

# ── Win32 ──
user32 = ctypes.windll.user32

class POINT(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]

class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]

def get_foreground():
    return user32.GetForegroundWindow()

def get_window_rect(hwnd):
    r = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    return (r.left, r.top, r.right, r.bottom)

def find_game():
    """Encontra a janela do CABAL (exclui nossa GUI)."""
    result = {"hwnd": None}
    def enum_cb(hwnd, _):
        if user32.IsWindowVisible(hwnd):
            buf = ctypes.create_unicode_buffer(256)
            user32.GetWindowTextW(hwnd, buf, 256)
            title = buf.value
            # Exclui nossa GUI e janelas irrelevantes
            if "CABAL Login" in title:
                return True
            if "CABAL" in title.upper():
                result["hwnd"] = hwnd
                print("Janela encontrada: '%s' (hwnd=%s)" % (title, hwnd))
                return False
        return True
    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows(WNDENUMPROC(enum_cb), 0)
    if result["hwnd"]:
        return result["hwnd"]
    # Fallback: janela ativa
    hwnd = get_foreground()
    if hwnd:
        buf = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, buf, 256)
        print("AVISO: CABAL nao encontrado. Janela ativa: '%s'" % buf.value)
        return hwnd
    return None

def screenshot_window(hwnd):
    """Captura a janela do jogo como PIL Image."""
    left, top, right, bottom = get_window_rect(hwnd)
    w = right - left
    h = bottom - top
    with mss.MSS() as sct:
        shot = sct.grab({"left": left, "top": top, "width": w, "height": h})
    return Image.frombytes("RGB", shot.size, shot.bgra, "raw", "BGRX"), (left, top)

def load_digit_refs():
    """Carrega imagens 0-9.jpg como referência."""
    refs = {}
    for d in range(10):
        path = os.path.join(IMG_DIR, "%d.jpg" % d)
        if os.path.exists(path):
            refs[d] = Image.open(path).convert("RGB")
    return refs

def main():
    print("=" * 60)
    print("TESTE DE CONFIANCA - DIGITOS DA SUBSENHA")
    print("=" * 60)

    hwnd = find_game()
    if not hwnd:
        print("ERRO: Nenhuma janela ativa")
        return

    print("\nCapturando screenshot da janela...")
    screen_pil, (wx, wy) = screenshot_window(hwnd)
    screen = np.array(screen_pil)
    screen_bgr = cv2.cvtColor(screen, cv2.COLOR_RGB2BGR)
    print("Screenshot: %dx%d" % (screen_bgr.shape[1], screen_bgr.shape[0]))

    refs = load_digit_refs()
    print("Referências carregadas: %d dígitos" % len(refs))

    print("\n" + "-" * 60)
    print("MATCH POR DIGITO (TODAS as ocorrencias na tela)")
    print("-" * 60)

    for d in range(10):
        if d not in refs:
            print("  %d: sem referência" % d)
            continue

        ref_img = np.array(refs[d].convert("RGB"))
        ref_bgr = cv2.cvtColor(ref_img, cv2.COLOR_RGB2BGR)
        rh, rw = ref_bgr.shape[:2]

        result = cv2.matchTemplate(screen_bgr, ref_bgr, cv2.TM_CCOEFF_NORMED)
        _, max_val, _, max_loc = cv2.minMaxLoc(result)

        # Coordenadas absolutas na tela
        ax = wx + max_loc[0] + rw // 2
        ay = wy + max_loc[1] + rh // 2

        # Conta quantas ocorrências acima de cada threshold
        count_95 = int(np.sum(result >= 0.95))
        count_90 = int(np.sum(result >= 0.90))
        count_80 = int(np.sum(result >= 0.80))
        count_65 = int(np.sum(result >= 0.65))

        status = "[OK]" if max_val >= 0.95 else ("[BAIXO]" if max_val >= 0.65 else "[FALHA]")
        print("  %d: match=%.4f  pos=(%d,%d)  size=%dx%d  %s" % (
            d, max_val, ax, ay, rw, rh, status))
        print("      ocorrencias: >=95%%=%d  >=90%%=%d  >=80%%=%d  >=65%%=%d" % (
            count_95, count_90, count_80, count_65))

    print("\n" + "-" * 60)
    print("RESUMO")
    print("-" * 60)

    # Roda novamente e mostra resumo
    for d in range(10):
        if d not in refs:
            continue
        ref_img = np.array(refs[d].convert("RGB"))
        ref_bgr = cv2.cvtColor(ref_img, cv2.COLOR_RGB2BGR)
        result = cv2.matchTemplate(screen_bgr, ref_bgr, cv2.TM_CCOEFF_NORMED)
        _, max_val, _, _ = cv2.minMaxLoc(result)
        status = "[OK]" if max_val >= 0.95 else ("[BAIXO]" if max_val >= 0.65 else "[FALHA]")
        print("  Dígito %d: %.4f  %s" % (d, max_val, status))

    print("\nThreshold mínimo: 0.95 (95%)")
    print("Acima de 0.95 = detecção automática funciona")
    print("Abaixo de 0.95 = dialogo manual aparece")

if __name__ == "__main__":
    main()
