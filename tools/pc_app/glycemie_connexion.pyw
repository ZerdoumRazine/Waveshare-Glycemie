#!/usr/bin/env python3
# Windows configuration interface for the connected display.
# FR: Interface Windows de configuration de l'écran.
# Design note: Do not display, log or persist account passwords on the PC.
# EN: Provides the Windows USB setup interface; credential changes remain a confirmable trial.
# FR: Interface Windows de configuration USB avec essai réversible des accès.
"""Configuration de l'ecran WaveShare Glycemie depuis le PC (fenetre graphique Windows).

Ce programme sert a renseigner, essayer, enregistrer ou effacer les acces de l'ecran (reseau Wi-Fi et
compte Dexcom Share) par le cable USB, sans rien installer et sans recompiler le firmware.

Regle de securite appliquee ici comme sur l'ecran :
    garder les acces precedents -> essayer les nouveaux -> enregistrer seulement apres reussite
    ET confirmation. Un echec, un abandon ou un debranchement laisse l'appareil avec ses acces
    precedents.

Ce programme ne va jamais sur Internet, n'ecrit aucun mot de passe dans un fichier et n'affiche jamais
un mot de passe en clair (sauf si vous cochez "Afficher"). Il n'y a ni compte, ni abonnement, ni service
en ligne : les acces saisis servent uniquement aux connexions normales de l'appareil (sa box Wi-Fi et
les serveurs Dexcom Share).

Le dialogue avec l'appareil est entierement assure par tools/assistant_config.py (meme code que
l'assistant en ligne de commande).
"""
import os
import queue
import sys
import threading
import tkinter as tk
from tkinter import messagebox, ttk

# --- import de l'assistant (tools/assistant_config.py), que le programme soit lance depuis les
#     sources ou depuis l'executable fabrique par PyInstaller -----------------------------------
ICI = os.path.dirname(os.path.abspath(__file__))
for candidat in (os.path.join(ICI, '..'), ICI, getattr(sys, '_MEIPASS', ICI)):
    if os.path.isfile(os.path.join(candidat, 'assistant_config.py')) and candidat not in sys.path:
        sys.path.insert(0, candidat)

import assistant_config as ac  # noqa: E402

TITRE = 'WaveShare Glycémie — connexion'
# Libelles accentues pour la fenetre ; les codes ("ous", "us", "jp") restent ceux du firmware.
REGIONS_FR = {'ous': 'Europe / hors Amérique du Nord', 'us': 'Amérique du Nord', 'jp': 'Japon'}
LARGEUR_CHAMP = 34

AIDE_DEXCOM = ("Utilisez le compte Dexcom qui PARTAGE les mesures (celui de l'application Dexcom),\n"
               "pas celui d'un suiveur. Le partage doit être activé dans l'application Dexcom.")


class Fenetre(object):
    # Build controls with passwords hidden by default.
    def __init__(self, racine):
        self.racine = racine
        self.file = queue.Queue()        # messages venant du fil de discussion avec l'appareil
        self.occupe = False
        self.essai_reussi = False
        racine.title(TITRE)
        racine.minsize(820, 680)
        racine.geometry('980x980')
        racine.columnconfigure(0, weight=1)

        style = ttk.Style()
        for nom in ('vista', 'winnative', 'clam'):
            if nom in style.theme_names():
                style.theme_use(nom)
                break
        style.configure('Grand.TButton', padding=(14, 10))
        style.configure('Titre.TLabel', font=('Segoe UI', 11, 'bold'))

        self._zone_appareil(racine, 0)
        self._zone_wifi(racine, 1)
        self._zone_dexcom(racine, 2)
        self._zone_actions(racine, 3)
        self._zone_etat(racine, 4)

        self.rafraichir_ports()
        self._tache = self.racine.after(120, self._vider_file)
        racine.protocol('WM_DELETE_WINDOW', self.fermer)

    # ---------------------------------------------------------------- interface
    def _cadre(self, parent, ligne, titre):
        cadre = ttk.LabelFrame(parent, text=titre, padding=12)
        cadre.grid(row=ligne, column=0, sticky='ew', padx=14, pady=(12 if ligne == 0 else 8, 0))
        cadre.columnconfigure(1, weight=1)
        return cadre

    def _zone_appareil(self, parent, ligne):
        cadre = self._cadre(parent, ligne, '1. Appareil')
        ttk.Label(cadre, text="Branchez l'écran au PC avec son câble USB-C, puis choisissez-le :")\
            .grid(row=0, column=0, columnspan=3, sticky='w')
        self.port = tk.StringVar()
        self.liste_ports = ttk.Combobox(cadre, textvariable=self.port, state='readonly', width=52)
        self.liste_ports.grid(row=1, column=0, columnspan=2, sticky='ew', pady=(8, 0))
        ttk.Button(cadre, text='Rechercher', command=self.rafraichir_ports)\
            .grid(row=1, column=2, sticky='e', padx=(8, 0), pady=(8, 0))
        self.btn_etat = ttk.Button(cadre, text="Lire l'état de l'appareil", command=self.lire_etat)
        self.btn_etat.grid(row=2, column=0, sticky='w', pady=(8, 0))
        self.etiquette_etat = ttk.Label(cadre, text='', foreground='#444')
        self.etiquette_etat.grid(row=3, column=0, columnspan=3, sticky='w', pady=(8, 0))

    def _zone_wifi(self, parent, ligne):
        cadre = self._cadre(parent, ligne, '2. Réseau Wi-Fi (2,4 GHz)')
        ttk.Label(cadre, text='Nom du réseau').grid(row=0, column=0, sticky='w')
        self.ssid = tk.StringVar()
        ttk.Entry(cadre, textvariable=self.ssid, width=LARGEUR_CHAMP)\
            .grid(row=0, column=1, sticky='w', padx=(10, 0))
        ttk.Label(cadre, text='Mot de passe').grid(row=1, column=0, sticky='w', pady=(8, 0))
        self.mdp_wifi = tk.StringVar()
        self.champ_mdp_wifi = ttk.Entry(cadre, textvariable=self.mdp_wifi, width=LARGEUR_CHAMP, show='•')
        self.champ_mdp_wifi.grid(row=1, column=1, sticky='w', padx=(10, 0), pady=(8, 0))
        self.voir_wifi = tk.BooleanVar(value=False)
        ttk.Checkbutton(cadre, text='Afficher', variable=self.voir_wifi,
                        command=lambda: self.champ_mdp_wifi.configure(
                            show='' if self.voir_wifi.get() else '•'))\
            .grid(row=1, column=2, sticky='w', padx=(8, 0), pady=(8, 0))

    def _zone_dexcom(self, parent, ligne):
        cadre = self._cadre(parent, ligne, '3. Compte Dexcom Share')
        ttk.Label(cadre, text=AIDE_DEXCOM, foreground='#444')\
            .grid(row=0, column=0, columnspan=3, sticky='w')
        ttk.Label(cadre, text='Identifiant').grid(row=1, column=0, sticky='w', pady=(8, 0))
        self.user = tk.StringVar()
        ttk.Entry(cadre, textvariable=self.user, width=LARGEUR_CHAMP)\
            .grid(row=1, column=1, sticky='w', padx=(10, 0), pady=(8, 0))
        ttk.Label(cadre, text='Mot de passe').grid(row=2, column=0, sticky='w', pady=(8, 0))
        self.mdp_dex = tk.StringVar()
        self.champ_mdp_dex = ttk.Entry(cadre, textvariable=self.mdp_dex, width=LARGEUR_CHAMP, show='•')
        self.champ_mdp_dex.grid(row=2, column=1, sticky='w', padx=(10, 0), pady=(8, 0))
        self.voir_dex = tk.BooleanVar(value=False)
        ttk.Checkbutton(cadre, text='Afficher', variable=self.voir_dex,
                        command=lambda: self.champ_mdp_dex.configure(
                            show='' if self.voir_dex.get() else '•'))\
            .grid(row=2, column=2, sticky='w', padx=(8, 0), pady=(8, 0))
        ttk.Label(cadre, text='Région').grid(row=3, column=0, sticky='w', pady=(8, 0))
        self.region = tk.StringVar(value=REGIONS_FR['ous'])
        ttk.Combobox(cadre, textvariable=self.region, state='readonly', width=32,
                     values=[REGIONS_FR[ac.REGIONS[k][0]] for k in sorted(ac.REGIONS)])\
            .grid(row=3, column=1, sticky='w', padx=(10, 0), pady=(8, 0))

    def _zone_actions(self, parent, ligne):
        cadre = self._cadre(parent, ligne, '4. Essayer, puis enregistrer')
        ttk.Label(cadre, text="Les accès déjà présents dans l'écran sont conservés jusqu'à "
                              "l'enregistrement.", foreground='#444')\
            .grid(row=0, column=0, columnspan=4, sticky='w')
        self.btn_essai = ttk.Button(cadre, text='Essayer la connexion', style='Grand.TButton',
                                    command=self.essayer)
        self.btn_essai.grid(row=1, column=0, sticky='w', pady=(10, 0))
        self.btn_enregistrer = ttk.Button(cadre, text='Enregistrer', style='Grand.TButton',
                                          command=self.enregistrer, state='disabled')
        self.btn_enregistrer.grid(row=1, column=1, sticky='w', padx=(10, 0), pady=(10, 0))
        self.btn_annuler = ttk.Button(cadre, text="Annuler l'essai", command=self.annuler,
                                      state='disabled')
        self.btn_annuler.grid(row=1, column=2, sticky='w', padx=(10, 0), pady=(10, 0))
        ttk.Button(cadre, text='Effacer les accès de l\'écran…', command=self.effacer)\
            .grid(row=1, column=3, sticky='e', padx=(10, 0), pady=(10, 0))

    def _zone_etat(self, parent, ligne):
        cadre = self._cadre(parent, ligne, 'Ce qui se passe')
        parent.rowconfigure(ligne, weight=1)
        cadre.rowconfigure(0, weight=1)
        self.journal = tk.Text(cadre, height=9, wrap='word', state='disabled',
                               background='#fbfbfb', relief='flat')
        self.journal.grid(row=0, column=0, columnspan=2, sticky='nsew')
        barre = ttk.Scrollbar(cadre, command=self.journal.yview)
        barre.grid(row=0, column=2, sticky='ns')
        self.journal.configure(yscrollcommand=barre.set)
        self.progression = ttk.Progressbar(cadre, mode='indeterminate')
        self.progression.grid(row=1, column=0, columnspan=3, sticky='ew', pady=(8, 0))

    # ---------------------------------------------------------------- utilitaires
    def dire(self, texte):
        self.journal.configure(state='normal')
        self.journal.insert('end', texte + '\n')
        self.journal.see('end')
        self.journal.configure(state='disabled')

    # List detected serial ports without probing or resetting the user's device.
    def rafraichir_ports(self):
        self.ports = ac.lister_ports()
        libelles = ['%s — %s' % (nom, description or 'port série') for nom, description in self.ports]
        self.liste_ports.configure(values=libelles)
        if not self.ports:
            self.port.set('')
            self.dire("Aucun port série trouvé. Branchez le câble USB-C sur la prise de la carte, "
                      "attendez deux secondes, puis appuyez sur « Rechercher ».")
            return
        suggere = ac.trouver_port()
        if suggere is not None:
            for i, (nom, _) in enumerate(self.ports):
                if nom == suggere:
                    self.liste_ports.current(i)
                    break
        elif len(self.ports) == 1:
            self.liste_ports.current(0)
        else:
            # Plusieurs appareils : c'est a l'utilisateur de choisir, jamais au programme.
            self.port.set('')
            self.dire('%d ports série trouvés : choisissez celui de l\'écran dans la liste. '
                      'En cas de doute, débranchez le câble, appuyez sur « Rechercher », '
                      'rebranchez-le et regardez le port qui apparaît.' % len(self.ports))
        if self.port.get():
            self.dire('Appareil proposé : %s' % self.port.get())

    def port_choisi(self):
        valeur = self.port.get()
        if not valeur:
            messagebox.showinfo(TITRE, "Choisissez d'abord l'appareil dans la liste.")
            return None
        return valeur.split(' ')[0]

    def region_choisie(self):
        for code, libelle in REGIONS_FR.items():
            if libelle == self.region.get():
                return code
        return 'ous'

    def _boutons(self, actifs):
        etat = 'normal' if actifs else 'disabled'
        for bouton in (self.btn_essai, self.btn_etat):
            bouton.configure(state=etat)
        self.btn_enregistrer.configure(state='normal' if actifs and self.essai_reussi else 'disabled')
        self.btn_annuler.configure(state='normal' if actifs and self.essai_reussi else 'disabled')

    # Keep serial I/O off the Tkinter thread so the interface remains responsive.
    def lancer(self, travail, titre):
        """Execute 'travail(app, dire)' dans un fil separe : la fenetre ne se figer jamais."""
        if self.occupe:
            return
        port = self.port_choisi()
        if port is None:
            return
        self.occupe = True
        self._boutons(False)
        self.progression.start(12)
        self.dire('\n--- %s ---' % titre)

        def fil():
            try:
                transport = ac.ouvrir_port(port)
            except Exception as err:                                  # noqa: BLE001
                self.file.put(('dire', "Impossible d'ouvrir %s : %s" % (port, err)))
                self.file.put(('dire', "Fermez tout autre logiciel qui utilise ce port "
                                       "(moniteur série, idf.py monitor), puis réessayez."))
                self.file.put(('fini', None))
                return
            app = ac.Appareil(transport)
            try:
                travail(app, lambda t: self.file.put(('dire', t)))
            except Exception as err:                                  # noqa: BLE001
                self.file.put(('dire', 'Interrompu : %s' % err))
            finally:
                try:
                    app.close()
                except Exception:                                     # noqa: BLE001
                    pass
                self.file.put(('fini', None))

        threading.Thread(target=fil, daemon=True).start()

    # Render worker results on the main Tkinter thread only.
    def _vider_file(self):
        try:
            while True:
                genre, valeur = self.file.get_nowait()
                if genre == 'dire':
                    self.dire(valeur)
                elif genre == 'etat':
                    self.etiquette_etat.configure(text=valeur)
                elif genre == 'essai':
                    self.essai_reussi = bool(valeur)
                elif genre == 'fini':
                    self.occupe = False
                    self.progression.stop()
                    self._boutons(True)
        except queue.Empty:
            pass
        self._tache = self.racine.after(120, self._vider_file)

    def fermer(self):
        """Fermeture propre : plus aucune tache en attente, puis la fenetre disparait."""
        if self._tache is not None:
            self.racine.after_cancel(self._tache)
            self._tache = None
        self.racine.destroy()

    # ---------------------------------------------------------------- actions
    def lire_etat(self):
        def travail(app, dire):
            etat = app.etat()
            if etat is None:
                dire("L'appareil ne répond pas. Vérifiez qu'il est allumé, que le câble est bien "
                     "enfoncé, et qu'aucun autre logiciel n'utilise ce port.")
                self.file.put(('etat', 'Appareil muet'))
                return
            dire('Réponse : %s' % etat)
            lisible = []
            reseau = etat.split('wifi="', 1)[-1].split('"', 1)[0] if 'wifi="' in etat else '?'
            compte = etat.split('dexcom="', 1)[-1].split('"', 1)[0] if 'dexcom="' in etat else '?'
            lisible.append('Réseau : %s' % (reseau if reseau != '-' else 'non renseigné'))
            lisible.append('Compte Dexcom : %s' % (compte if compte != '-' else 'non renseigné'))
            if 'configure=1' in etat:
                lisible.append('configuré')
            if 'demo=1' in etat:
                lisible.append('mode démonstration')
            if 'essai=1' in etat:
                lisible.append("essai en cours (non enregistré)")
            self.file.put(('etat', '   |   '.join(lisible)))
            if reseau not in ('?', '-'):
                dire("Cet écran a déjà des accès. Ils seront conservés tant que vous n'aurez pas "
                     "appuyé sur « Enregistrer ».")
        self.lancer(travail, "Lecture de l'état")

    # Test new details on the screen before enabling the Save button.
    def essayer(self):
        ssid, mdp_wifi = self.ssid.get().strip(), self.mdp_wifi.get()
        user, mdp_dex = self.user.get().strip(), self.mdp_dex.get()
        region = self.region_choisie()
        if not ssid or not user or not mdp_dex:
            messagebox.showinfo(TITRE, 'Renseignez le nom du réseau, l\'identifiant Dexcom et son '
                                       'mot de passe.')
            return
        if '|' in ssid + mdp_wifi + user + mdp_dex:
            messagebox.showwarning(TITRE, "Le caractère | n'est pas accepté par cet assistant : "
                                          "utilisez l'assistant tactile de l'écran.")
            return
        self.essai_reussi = False

        def travail(app, dire):
            if app.etat() is None:
                dire("L'appareil ne répond pas : rien n'a été envoyé.")
                return
            dire('Envoi du réseau Wi-Fi…')
            app.envoyer('ident_wifi %s|%s' % (ssid, mdp_wifi))
            reponse = [l for l in app.lire(4, 'IDENT wifi ') if l.startswith('IDENT wifi ')]
            if not reponse or "a l'essai" not in reponse[0]:
                dire('Refusé par l\'appareil : %s' % (reponse[0] if reponse else 'pas de réponse'))
                app.envoyer('ident_annuler')
                return
            dire('Envoi du compte Dexcom…')
            app.envoyer('ident_dexcom %s|%s|%s' % (user, mdp_dex, region))
            reponse = [l for l in app.lire(4, 'IDENT dexcom ') if l.startswith('IDENT dexcom ')]
            if not reponse or "a l'essai" not in reponse[0]:
                dire('Refusé par l\'appareil : %s' % (reponse[0] if reponse else 'pas de réponse'))
                app.envoyer('ident_annuler')
                return
            dire("Essai de connexion (Wi-Fi, Internet, Dexcom) : jusqu'à une minute…")
            etape, detail = ac.ETAT_INACTIF, ''
            app.envoyer('ident_test')
            app.lire(1, 'IDENT essai de connexion')
            vu = set()
            import re as _re
            import time as _time
            for _ in range(30):
                app.envoyer('ident_etat')
                for ligne in app.lire(1.5, 'IDENT essai='):
                    trouve = _re.search(r'essai=(\d+) detail=(\S*)', ligne)
                    if trouve:
                        etape, detail = int(trouve.group(1)), trouve.group(2)
                if etape in (ac.ETAT_REUSSI, ac.ETAT_ECHEC):
                    break
                if etape == ac.ETAT_EN_COURS and detail not in vu:
                    vu.add(detail)
                    dire('   … %s' % ac.ETAPES.get(detail, detail))
                _time.sleep(0.5)
            if etape == ac.ETAT_REUSSI:
                dire('Tout fonctionne. Appuyez sur « Enregistrer » pour garder ces accès dans '
                     "l'écran, ou sur « Annuler l'essai » pour revenir aux précédents.")
                self.file.put(('essai', True))
                return
            if etape == ac.ETAT_ECHEC:
                dire(ac.ECHECS.get(detail, 'Échec (%s).' % detail))
            else:
                dire("Pas de résultat : regardez l'écran de l'appareil.")
            dire("Les accès précédents de l'écran sont conservés : rien n'a été modifié.")
            app.envoyer('ident_annuler')
            app.lire(2, 'IDENT annule')
        self.lancer(travail, 'Essai des accès')

    # Persist only a successfully tested configuration, after confirmation.
    def enregistrer(self):
        if not self.essai_reussi:
            messagebox.showinfo(TITRE, "Faites d'abord un essai réussi : on n'enregistre jamais des "
                                       'accès qui ne fonctionnent pas.')
            return
        if not messagebox.askyesno(TITRE, 'Enregistrer ces accès dans l\'écran ?\n\n'
                                          'Les accès précédents seront remplacés. '
                                          "L'appareil devra être redémarré."):
            return

        def travail(app, dire):
            app.envoyer('ident_enregistrer')
            reponse = [l for l in app.lire(4, 'IDENT enregistrer') if l.startswith('IDENT enregistrer')]
            if reponse and ' ok' in reponse[0]:
                dire("C'est enregistré. Débranchez l'écran une seconde puis rebranchez-le : il se "
                     'connectera avec ces accès.')
                self.file.put(('essai', False))
                self.mdp_wifi.set('')
                self.mdp_dex.set('')
            else:
                dire('Enregistrement impossible : %s' % (reponse[0] if reponse else 'pas de réponse'))
                dire('Les accès précédents sont conservés.')
        self.lancer(travail, 'Enregistrement')

    def annuler(self):
        def travail(app, dire):
            app.envoyer('ident_annuler')
            app.lire(2, 'IDENT annule')
            dire("Essai abandonné : l'écran garde ses accès précédents.")
            self.file.put(('essai', False))
        self.lancer(travail, "Abandon de l'essai")

    # Do not report deletion as successful until the device acknowledges it.
    def effacer(self):
        if not messagebox.askyesno(TITRE, "Effacer le réseau Wi-Fi et le compte Dexcom de l'écran ?\n\n"
                                          "L'assistant tactile reprendra au prochain démarrage. "
                                          'Les réglages, les sons et les dates de consommables ne '
                                          'sont pas touchés.'):
            return

        def travail(app, dire):
            app.envoyer('ident_effacer')
            reponse = [l for l in app.lire(3, 'IDENT efface') if l.startswith('IDENT efface')]
            dire(reponse[0] if reponse else "Pas de réponse de l'appareil.")
            if reponse and reponse[0].startswith('IDENT efface ok'):
                dire("Redémarrez l'écran : l'assistant tactile reprendra. Le mode démonstration "
                     'reste disponible sans compte.')
            else:
                dire("Effacement non confirmé : les accès actuels ne doivent pas être considérés "
                     "comme effacés. Ne redémarrez pas pour le moment.")
        self.lancer(travail, 'Effacement des accès')


def ecran_haute_definition():
    """Ecrans 4K / affichage agrandi : sans cela, Windows etire la fenetre et tout est flou."""
    if sys.platform != 'win32':
        return
    try:
        import ctypes
        try:
            ctypes.windll.shcore.SetProcessDpiAwareness(1)   # Windows 8.1 et suivants
        except (AttributeError, OSError):
            ctypes.windll.user32.SetProcessDPIAware()        # Windows 7
    except Exception:                                        # noqa: BLE001
        pass


def main():
    ecran_haute_definition()
    racine = tk.Tk()
    try:
        racine.iconbitmap(default='')
    except tk.TclError:
        pass
    fenetre = Fenetre(racine)
    fenetre.dire('Bienvenue. Cette fenêtre configure l\'écran WaveShare Glycémie par le câble USB.')
    fenetre.dire("Rien n'est envoyé sur Internet, et aucun mot de passe n'est écrit sur ce PC.")
    racine.mainloop()
    return 0


if __name__ == '__main__':
    sys.exit(main())
