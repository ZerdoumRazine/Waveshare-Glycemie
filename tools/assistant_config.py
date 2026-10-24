#!/usr/bin/env python3
# Configures the screen over USB without requiring a firmware rebuild.
#
# Design note: Passwords are sent to the device, not stored in a PC configuration file.
# Configures the device by USB when touchscreen entry is inconvenient.
#
"""Cable Configuration Assistant USB (relief solution a l'assistant de l'touch screen).

Use if the finger grab poses a problem (long password, keyboard has an impractical screen) or if
The tactile doesn't answer. The device must be switched on and connected to the PC by its cable USB-C.

This script never reads the memory of the PC or that of the'appareil, n'write no password in file
and does not display what you type for a password. Identifiers leave via cable and are
recorded by l'appareil lui-meme (espace NVS "ident"). Rien n'is sent over the Internet.

No installation is required under Windows: the serial port is opened directly (ctypes). If
Pyserial library is present, it is used instead (useful under Linux and macOS).

Usage :
    python tools\\assistant_config.py                 (port detecte, sinon --port COM7)
    python tools\\assistant_config.py --lister lists the serial ports seen by the PC
    python tools\\assistant_config.py --etat displays only what is already saved
    python tools\\assistant_config.py -- Clear device identifiers
"""
import argparse
import getpass
import re
import subprocess
import sys
import time

REGIONS = {'1': ('ous', 'Europe / hors Amerique du Nord'), '2': ('us', 'Amerique du Nord'),
           '3': ('jp', 'Japon')}

# etats renvoyes par "ident_etat" (gly_test_t in main/glucose_service.h)
ETAT_INACTIF, ETAT_EN_COURS, ETAT_REUSSI, ETAT_ECHEC = 0, 1, 2, 3
ETAPES = {
    'demarrage': 'demarrage',
    'wifi': 'connexion au Wi-Fi',
    'internet': "acces a Internet et mise a l'heure",
    'dexcom': 'connexion au compte Dexcom',
}
ECHECS = {
    'wifi': "Connexion Wi-Fi impossible : verifiez le mot de passe, et que la box emette bien en 2,4 GHz.",
    'internet': "Wi-Fi connecte, mais pas d'acces a Internet (box sans Internet, ou portail a accepter).",
    'dexcom_vide': "Compte Dexcom incomplet : identifiant ou mot de passe manquant.",
    'dexcom_refuse': "Identifiant ou mot de passe Dexcom refuse. Verifiez-les dans l'application Dexcom.",
    'dexcom_bloque': "Compte bloque par Dexcom apres trop d'essais : attendez avant de reessayer.",
    'dexcom_reseau': "Serveur Dexcom injoignable pour l'instant : reessayez plus tard.",
    'dexcom_autre': "Refus inattendu du serveur Dexcom : voir les traces de l'appareil.",
    'memoire': "Memoire insuffisante sur l'appareil : redemarrez-le puis reessayez.",
}

BAUD = 115200  # console ESP-IDF


# --------------------------------------------------------------------------------------------------
# Access to the port serie : pyserial if available, otherwise theAPI Windows directly (no installation)
# --------------------------------------------------------------------------------------------------
try:
    import serial                      # noqa: F401
    from serial.tools import list_ports
    PYSERIAL = True
except ImportError:
    PYSERIAL = False


class PortPySerial(object):
    def __init__(self, nom):
        self.s = serial.Serial()
        self.s.port = nom
        self.s.baudrate = BAUD
        self.s.timeout = 0.2
        self.s.dtr = False   # do not restart the device by opening the port
        self.s.rts = False
        self.s.open()

    def write(self, data):
        self.s.write(data)

    def read(self, taille):
        return self.s.read(taille)

    def close(self):
        self.s.close()


class PortWin32(object):
    """PortAPI Windows (kernel32), no dependance to install."""

    def __init__(self, nom):
        import ctypes
        from ctypes import wintypes
        self.ct = ctypes
        self.wt = wintypes
        k = ctypes.WinDLL('kernel32', use_last_error=True)
        self.k = k

        class DCB(ctypes.Structure):
            _fields_ = [('DCBlength', wintypes.DWORD), ('BaudRate', wintypes.DWORD),
                        ('bits', wintypes.DWORD), ('wReserved', wintypes.WORD),
                        ('XonLim', wintypes.WORD), ('XoffLim', wintypes.WORD),
                        ('ByteSize', ctypes.c_ubyte), ('Parity', ctypes.c_ubyte),
                        ('StopBits', ctypes.c_ubyte), ('XonChar', ctypes.c_char),
                        ('XoffChar', ctypes.c_char), ('ErrorChar', ctypes.c_char),
                        ('EofChar', ctypes.c_char), ('EvtChar', ctypes.c_char),
                        ('wReserved1', wintypes.WORD)]

        class TIMEOUTS(ctypes.Structure):
            _fields_ = [('ReadIntervalTimeout', wintypes.DWORD),
                        ('ReadTotalTimeoutMultiplier', wintypes.DWORD),
                        ('ReadTotalTimeoutConstant', wintypes.DWORD),
                        ('WriteTotalTimeoutMultiplier', wintypes.DWORD),
                        ('WriteTotalTimeoutConstant', wintypes.DWORD)]

        chemin = nom if nom.startswith('\\\\') else '\\\\.\\' + nom
        self.h = k.CreateFileW(chemin, 0xC0000000, 0, None, 3, 0, None)
        if self.h == wintypes.HANDLE(-1).value or self.h == -1:
            raise IOError('ouverture de %s impossible (erreur Windows %d)'
                          % (nom, ctypes.get_last_error()))
        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        if not k.GetCommState(self.h, ctypes.byref(dcb)):
            k.CloseHandle(self.h)
            raise IOError('GetCommState a echoue sur %s' % nom)
        dcb.BaudRate = BAUD
        dcb.ByteSize = 8
        dcb.Parity = 0      # none
        dcb.StopBits = 0    # 1 bit
        dcb.bits = 0x0001   # binary; DTR et RTS desactive, no flow control
        if not k.SetCommState(self.h, ctypes.byref(dcb)):
            k.CloseHandle(self.h)
            raise IOError('SetCommState a echoue sur %s' % nom)
        to = TIMEOUTS(50, 0, 200, 0, 2000)
        k.SetCommTimeouts(self.h, ctypes.byref(to))
        k.PurgeComm(self.h, 0x000F)  # emptys entry and exit buffers

    def write(self, data):
        ecrit = self.wt.DWORD(0)
        self.k.WriteFile(self.h, data, len(data), self.ct.byref(ecrit), None)
        return ecrit.value

    def read(self, taille):
        buf = self.ct.create_string_buffer(taille)
        lu = self.wt.DWORD(0)
        if not self.k.ReadFile(self.h, buf, taille, self.ct.byref(lu), None):
            return b''
        return buf.raw[:lu.value]

    def close(self):
        self.k.CloseHandle(self.h)


def noms_lisibles():
    """{COM7: 'USB-Enhanced-SERIAL CH343'Names posters by Windows, without opening anything.

    Go through PowerShell (no library to install). If this fails, we are content with the
    Technical names of the registry: detection continues to work.
    """
    if sys.platform != 'win32':
        return {}
    requete = ("Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -like '*(COM*' } | "
               "ForEach-Object { $_.Name }")
    try:
        sortie = subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command', requete],
                                capture_output=True, text=True, timeout=25,
                                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0)).stdout
    except (OSError, subprocess.SubprocessError):
        return {}
    noms = {}
    for ligne in sortie.splitlines():
        trouve = re.search(r'^(.*)\((COM\d+)\)\s*$', ligne.strip())
        if trouve:
            noms[trouve.group(2)] = trouve.group(1).strip()
    return noms


def lister_ports():
    """[(name, description)] of the serial ports, without opening anything."""
    if PYSERIAL:
        return [(p.device, ('%s %s' % (p.description or '', p.manufacturer or '')).strip())
                for p in list_ports.comports()]
    if sys.platform != 'win32':
        return []
    try:
        import winreg
    except ImportError:
        return []
    lisibles = noms_lisibles()
    ports = []
    try:
        cle = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r'HARDWARE\DEVICEMAP\SERIALCOMM')
    except OSError:
        return []
    try:
        i = 0
        while True:
            try:
                nom_pilote, nom_port, _ = winreg.EnumValue(cle, i)
            except OSError:
                break
            ports.append((nom_port, lisibles.get(nom_port, nom_pilote)))
            i += 1
    finally:
        winreg.CloseKey(cle)
    return ports


# Names given by Windows to link chips USB used by these cards.
PUCES_USB = ('CH343', 'CH34', 'USB-SERIAL', 'USB-ENHANCED-SERIAL', 'CP210', 'FTDI', 'FT232',
             'USB SERIAL DEVICE', 'USBSER', 'JTAG')


def trouver_port():
    """Most likely port, or None s'il y a un doute : le choix revient alors a l'user."""
    ports = lister_ports()
    probables = [nom for nom, description in ports
                 if any(p in (description or '').upper() for p in PUCES_USB)]
    if len(probables) == 1:
        return probables[0]
    if not probables and len(ports) == 1:
        return ports[0][0]
    return None   # zero or more candidates: never choose the user's place


# Open the serial port without toggling DTR/RTS and unintentionally rebooting the board.
def ouvrir_port(nom):
    if PYSERIAL:
        return PortPySerial(nom)
    if sys.platform == 'win32':
        return PortWin32(nom)
    raise IOError("Sur ce systeme, installez pyserial :  python3 -m pip install pyserial")


# --------------------------------------------------------------------------------------------------
# Dialogue with the device
# --------------------------------------------------------------------------------------------------
class Appareil(object):
    def __init__(self, port):
        self.port = port          # objet write/read/close (injectable for testing)
        self.reste = b''

    def envoyer(self, commande):
        self.port.write((commande + '\n').encode('utf-8'))

    def lire(self, duree, motif=None):
        """Lines received during 'duree' seconds; s'arrete des qu'one line contains 'motif'."""
        fin = time.time() + duree
        lignes = []
        while True:
            self.reste += self.port.read(4096)
            while b'\n' in self.reste:
                brut, self.reste = self.reste.split(b'\n', 1)
                ligne = brut.decode('utf-8', errors='replace').strip()
                if ligne:
                    lignes.append(ligne)
                    if motif and motif in ligne:
                        return lignes
            if time.time() >= fin:
                return lignes

    def etat(self):
        self.envoyer('ident_info')
        for ligne in self.lire(3, 'IDENT wifi='):
            if ligne.startswith('IDENT wifi='):
                return ligne
        return None

    def close(self):
        self.port.close()


def demander(texte, defaut=''):
    reponse = input('%s%s : ' % (texte, (' [%s]' % defaut) if defaut else '')).strip()
    return reponse or defaut


def lire_mot_de_passe(texte):
    mdp = getpass.getpass(texte)
    if '|' in mdp:
        print("Le caractere | n'est pas accepte par cet assistant : utilisez l'assistant de l'ecran.")
        return None
    return mdp


# Try new credentials in RAM before asking the user to save them.
def configurer(app, demande=demander, mot_de_passe=lire_mot_de_passe):
    """Ask questions, try IDs, save them after success and confirmation.

    Access already present in'appareil sont conserves jusqu'a this confirmation: a failure,
    an abandonment or disconnection leaves them in place.
    """
    print('\n--- Reseau Wi-Fi (2,4 GHz uniquement) ---')
    ssid = demande('Nom du reseau')
    if not ssid or '|' in ssid:
        print('Nom de reseau vide ou contenant | : abandon.')
        return 1
    mdp = mot_de_passe('Mot de passe du Wi-Fi (invisible a la frappe) : ')
    if mdp is None:
        return 1
    app.envoyer('ident_wifi %s|%s' % (ssid, mdp))
    del mdp
    reponse = [l for l in app.lire(4, 'IDENT wifi ') if l.startswith('IDENT wifi ')]
    print(reponse[0] if reponse else "Pas de reponse de l'appareil.")
    if not reponse or "a l'essai" not in reponse[0]:
        app.envoyer('ident_annuler')
        return 1

    print('\n--- Compte Dexcom Share ---')
    print("Utilisez le compte qui PARTAGE les mesures (celui de l'application Dexcom), pas celui d'un suiveur.")
    for cle in sorted(REGIONS):
        print('  %s. %s' % (cle, REGIONS[cle][1]))
    region = REGIONS.get(demande('Region', '1'), REGIONS['1'])[0]
    user = demande('Identifiant Dexcom')
    if not user or '|' in user:
        print('Identifiant vide ou contenant | : abandon.')
        return 1
    mdp = mot_de_passe('Mot de passe Dexcom (invisible a la frappe) : ')
    if mdp is None:
        return 1
    if not mdp:
        print('Mot de passe vide : abandon.')
        return 1
    app.envoyer('ident_dexcom %s|%s|%s' % (user, mdp, region))
    del mdp
    reponse = [l for l in app.lire(4, 'IDENT dexcom ') if l.startswith('IDENT dexcom ')]
    print(reponse[0] if reponse else "Pas de reponse de l'appareil.")
    if not reponse or "a l'essai" not in reponse[0]:
        app.envoyer('ident_annuler')
        return 1

    if verifier(app) != 0:
        annuler(app)
        return 1
    return enregistrer(app, demande)


def annuler(app):
    """Abandonment: l'appareil revient a ses acces precedents (rien n'a was recorded)."""
    app.envoyer('ident_annuler')
    app.lire(2, 'IDENT annule')
    print('Les accès precedents de l\'appareil sont conservés : rien n\'a été modifié.')


def enregistrer(app, demande=demander):
    """Confirmation and registration: c'est la seule etape qui ecrit dans l'camera."""
    if demande('Enregistrer ces acces dans l\'appareil ? (oui/non)', 'oui').strip().lower() != 'oui':
        annuler(app)
        return 1
    app.envoyer('ident_enregistrer')
    reponse = [l for l in app.lire(4, 'IDENT enregistrer') if l.startswith('IDENT enregistrer')]
    print(reponse[0] if reponse else "Pas de reponse de l'appareil.")
    if not reponse or ' ok' not in reponse[0]:
        print("Enregistrement impossible : les acces precedents sont conserves.")
        return 1
    print("\nC'est enregistre. Redemarrez l'appareil (debranchez-le une seconde) pour qu'il se")
    print('connecte avec ces identifiants.')
    return 0


# Treat deletion as successful only when the device confirms it in NVS.
def effacer(app, demande=demander):
    """Clear IDs, after explicit confirmation. Returns 0 if erasure has occurred."""
    if demande('Effacer le reseau Wi-Fi et le compte Dexcom ? (oui/non)', 'non').strip().lower() != 'oui':
        print('Rien efface.')
        return 1
    app.envoyer('ident_effacer')
    reponse = [l for l in app.lire(3, 'IDENT efface') if l.startswith('IDENT efface')]
    print(reponse[0] if reponse else "Pas de reponse de l'appareil.")
    if not reponse or not reponse[0].startswith('IDENT efface ok'):
        print("L'effacement n'a PAS ete confirme : ne redemarrez pas pour le moment.")
        return 1
    print("Redemarrez l'appareil : l'assistant de l'ecran reprendra, et le mode demonstration")
    print('reste disponible sans compte.')
    return 0


# Poll the asynchronous network test without exposing either password.
def verifier(app, tours=30, pause=0.5):
    print('\n--- Verification ---')
    print("L'appareil essaie le Wi-Fi, l'heure, puis la connexion Dexcom (jusqu'a une minute).")
    app.envoyer('ident_test')
    app.lire(1, 'IDENT essai de connexion')
    etape, detail, vu = ETAT_INACTIF, '', set()
    for _ in range(tours):
        app.envoyer('ident_etat')
        for ligne in app.lire(1.5, 'IDENT essai='):
            trouve = re.search(r'essai=(\d+) detail=(\S*)', ligne)
            if trouve:
                etape, detail = int(trouve.group(1)), trouve.group(2)
        if etape in (ETAT_REUSSI, ETAT_ECHEC):
            break
        if etape == ETAT_EN_COURS and detail not in vu:
            vu.add(detail)
            print('  ... %s' % ETAPES.get(detail, detail))
        time.sleep(pause)

    if etape == ETAT_REUSSI:
        print('\nTout fonctionne.')
        return 0
    if etape != ETAT_ECHEC:
        print("\nPas de reponse de l'appareil. Regardez l'ecran : l'assistant tactile affiche le resultat.")
        return 1
    print('\n' + ECHECS.get(detail, "Echec (%s) : relancez l'assistant ou utilisez l'ecran tactile." % detail))
    return 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="Configuration de WaveShare Glycemie par le cable USB")
    ap.add_argument('--port', default=None, help='COM7 sous Windows, /dev/ttyACM0 sous Linux')
    ap.add_argument('--lister', action='store_true', help='liste les ports serie et quitte')
    ap.add_argument('--etat', action='store_true', help='affiche ce qui est enregistre et quitte')
    ap.add_argument('--effacer', action='store_true', help='efface les identifiants de l\'appareil')
    args = ap.parse_args(argv)

    if args.lister:
        ports = lister_ports()
        if not ports:
            print('Aucun port serie. Branchez le cable USB sur la prise de la carte.')
        for nom, description in ports:
            print('%-8s %s' % (nom, description))
        return 0

    port_nom = args.port or trouver_port()
    if port_nom is None:
        print("Port serie introuvable. Branchez l'appareil, puis relancez avec --port COM7")
        print("(la liste est donnee par :  python tools\\assistant_config.py --lister).")
        return 1
    print('Appareil sur %s' % port_nom)
    try:
        port = ouvrir_port(port_nom)
    except (IOError, OSError) as err:
        print('%s' % err)
        print("Fermez tout autre logiciel qui utilise ce port (moniteur serie, idf.py monitor).")
        return 1

    app = Appareil(port)
    try:
        etat = app.etat()
        if etat is None:
            print("L'appareil ne repond pas. Fermez tout autre logiciel qui utilise ce port, verifiez")
            print("le cable, puis relancez. L'appareil doit etre allume.")
            return 1
        print('Etat actuel : %s' % etat)
        if args.etat:
            return 0

        if args.effacer:
            return effacer(app)

        return configurer(app)
    finally:
        app.close()


if __name__ == '__main__':
    sys.exit(main())
