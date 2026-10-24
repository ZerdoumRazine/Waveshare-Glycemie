#!/usr/bin/env python3
# Captures the Windows configuration app for documentation.
#
# Design note: Show fictitious examples instead of personal credentials or glucose history.
# Captures the Windows configuration window for screenshots without changing device data.
#
"""Capture only the visible window of the Windows setup utility for tutorials.

Exclude surrounding desktop content, notifications and unrelated windows so
documentation screenshots do not reveal personal information."""
import ctypes
import os
import subprocess
import sys
import time

from ctypes import wintypes

try:
    from PIL import ImageGrab
except ImportError:
    print('Pillow est necessaire :  py -3 -m pip install pillow')
    sys.exit(2)

user32 = ctypes.windll.user32


# Use the real window bounds rather than an arbitrary crop.
def cadre_visible(hwnd):
    """Return the visible window rectangle without shadow or neighboring desktop pixels."""
    rect = wintypes.RECT()
    try:
        # DWMWA_EXTENDED_FRAME_BOUNDS = 9
        if ctypes.windll.dwmapi.DwmGetWindowAttribute(
                wintypes.HWND(hwnd), ctypes.c_uint(9), ctypes.byref(rect),
                ctypes.sizeof(rect)) == 0 and rect.right > rect.left:
            return rect
    except (AttributeError, OSError):
        pass
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    return rect


def fenetres_du_processus(pids):
    """Enumerate visible windows owned by any of the requested processes."""
    trouvees = []
    PROTO = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)

    def rappel(hwnd, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids and user32.IsWindowVisible(hwnd):
            longueur = user32.GetWindowTextLengthW(hwnd)
            if longueur > 0:
                buf = ctypes.create_unicode_buffer(longueur + 1)
                user32.GetWindowTextW(hwnd, buf, longueur + 1)
                trouvees.append((hwnd, buf.value))
        return True

    user32.EnumWindows(PROTO(rappel), 0)
    return trouvees


# Locate the application process before selecting a visible window.
def pids_du_nom(nom):
    """Find process IDs for this executable; PyInstaller may run parent and child processes."""
    sortie = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq %s' % nom, '/FO', 'CSV', '/NH'],
                            capture_output=True, text=True).stdout
    pids = set()
    for ligne in sortie.splitlines():
        morceaux = [m.strip('"') for m in ligne.split('","')]
        if len(morceaux) > 1 and morceaux[1].isdigit():
            pids.add(int(morceaux[1]))
    return pids


# Save a screenshot for documentation only; never include personal credentials.
def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    exe = os.path.abspath(sys.argv[1])
    sortie = os.path.abspath(sys.argv[2])
    attente = float(sys.argv[3]) if len(sys.argv) > 3 else 20.0
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except (AttributeError, OSError):
        user32.SetProcessDPIAware()

    if exe.lower().endswith(('.py', '.pyw')):
        nom = os.path.basename(sys.executable)   # window worn by python. exe
        commande = [sys.executable, exe]
    else:
        nom = os.path.basename(exe)
        commande = [exe]
        subprocess.run(['taskkill', '/F', '/IM', nom], capture_output=True)
    subprocess.Popen(commande, cwd=os.path.dirname(exe))

    fin = time.time() + attente
    cible = None
    while time.time() < fin and cible is None:
        time.sleep(0.4)
        for hwnd, titre in fenetres_du_processus(pids_du_nom(nom)):
            rect = cadre_visible(hwnd)
            if rect.right - rect.left > 300 and rect.bottom - rect.top > 300:
                cible = (hwnd, titre, rect)
                break
    if cible is None:
        if not exe.lower().endswith(('.py', '.pyw')):
            subprocess.run(['taskkill', '/F', '/IM', nom], capture_output=True)
        print("Aucune fenetre apres %.0f s : l'application n'a pas demarre." % attente)
        return 1

    hwnd, titre, _ = cible
    user32.SetForegroundWindow(hwnd)
    time.sleep(1.0)
    rect = cadre_visible(hwnd)
    image = ImageGrab.grab(bbox=(rect.left, rect.top, rect.right, rect.bottom), all_screens=True)
    os.makedirs(os.path.dirname(sortie), exist_ok=True)
    image.save(sortie)
    print('Fenetre « %s » %dx%d capturee : %s' % (titre, image.width, image.height, sortie))
    user32.PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE
    time.sleep(1.0)
    if not exe.lower().endswith(('.py', '.pyw')):
        subprocess.run(['taskkill', '/F', '/IM', nom], capture_output=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
