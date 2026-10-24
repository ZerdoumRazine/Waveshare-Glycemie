#!/usr/bin/env python3
# Checks the Windows configuration interface without hardware.
#
# These tests use simulations; they do not connect to a patient's device.
# Exercise the Windows Tkinter setup interface with simulated serial devices.
#
"""Controles de l'application Windows (tools/pc_app/glycemie_connexion.pyw), sans appareil branche.

La fenetre est reellement construite (Tk), puis fermee : ces essais verifient la logique de
l'interface, pas son apparence. Aucun port serie n'est ouvert : la liste des ports et l'ouverture
sont remplacees par des simulations.

Usage : python tests\\test_pc_app.py
"""
import importlib.util
import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SOURCE = os.path.join(ROOT, 'tools', 'pc_app', 'glycemie_connexion.pyw')
sys.path.insert(0, os.path.join(ROOT, 'tools'))

resultats = []


def check(nom, ok, detail=''):
    resultats.append((nom, bool(ok), detail))


# Import the Windows app code without launching it for a real user.
def charger():
    spec = importlib.util.spec_from_file_location('glycemie_connexion', SOURCE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# Check password masking and button states while preventing real serial communication.
def main():
    source = io.open(SOURCE, encoding='utf-8').read()

    # --- 1. Hygiene du code source ------------------------------------------------------------
    check("aucune adresse Internet dans l'application",
          'http://' not in source and 'https://' not in source)
    check("aucun fichier ouvert en ecriture (aucun mot de passe sur le disque)",
          "open(" not in source.replace('iconbitmap(default=', ''))
    check('les mots de passe sont masques dans les champs', source.count("show='•'") == 2)
    check("l'affichage en clair est un choix explicite de l'utilisateur",
          source.count("text='Afficher'") == 2)
    check('le dialogue avec l\'appareil est celui de l\'assistant deja teste',
          'import assistant_config as ac' in source)
    check('aucun identifiant en dur', '@gmail' not in source and 'razine' not in source.lower())
    check("effacement USB : afficher un redemarrage uniquement apres confirmation",
          "startswith('IDENT efface ok')" in source and
          "Effacement non confirmé" in source)

    try:
        import tkinter  # noqa: F401
    except ImportError:
        check('tkinter disponible pour construire la fenetre', False, 'tkinter absent')
        tkinter = None

    module = charger()
    check("l'application s'importe sans erreur", module is not None)
    check("elle reutilise les regions du firmware",
          sorted(c for c, _ in module.ac.REGIONS.values()) == ['jp', 'ous', 'us'])

    if tkinter is None:
        return rapport()

    # --- 2. Construction reelle de la fenetre --------------------------------------------------
    ports_simules = []
    module.ac.lister_ports = lambda: list(ports_simules)
    module.ac.trouver_port = lambda: (ports_simules[0][0] if len(ports_simules) == 1 else None)

    def fenetre_avec(ports):
        del ports_simules[:]
        ports_simules.extend(ports)
        racine = tkinter.Tk()
        racine.withdraw()                       # never displayed during testing
        f = module.Fenetre(racine)
        racine.update()
        return racine, f

    # un seul appareil : il est propose tout seul
    racine, f = fenetre_avec([('COM7', 'USB-Enhanced-SERIAL CH343')])
    check('un seul appareil : il est preselectionne', f.port.get().startswith('COM7'), f.port.get())
    check('le port retenu est bien le nom du port', f.port_choisi() == 'COM7', str(f.port_choisi()))
    check('le journal explique ce que fait la fenetre',
          'USB' in f.journal.get('1.0', 'end') or 'appareil' in f.journal.get('1.0', 'end').lower())
    f.fermer()

    # several devices: none is chosen in place of the user
    racine, f = fenetre_avec([('COM3', 'Peripherique Bluetooth'), ('COM7', 'Port serie')])
    check('plusieurs appareils : aucun choix arbitraire', f.port.get() == '', f.port.get())
    check('plusieurs appareils : la fenetre explique comment reconnaitre le bon',
          'choisissez' in f.journal.get('1.0', 'end').lower())
    check('la liste propose bien les deux ports',
          len(f.liste_ports.cget('values')) == 2)
    f.fermer()

    # aucun appareil : message clair, pas de plantage
    racine, f = fenetre_avec([])
    check('aucun appareil : message clair', 'Aucun port' in f.journal.get('1.0', 'end'))
    check('aucun appareil : rien de selectionne', f.port.get() == '')
    f.fermer()

    # --- 3. Regles de securite de l'interface --------------------------------------------------
    racine, f = fenetre_avec([('COM7', 'CH343')])
    check("le bouton Enregistrer est inactif tant qu'aucun essai n'a reussi",
          str(f.btn_enregistrer.cget('state')) == 'disabled')
    check("le bouton Annuler l'essai est inactif au depart",
          str(f.btn_annuler.cget('state')) == 'disabled')
    f.essai_reussi = True
    f._boutons(True)
    check("apres un essai reussi, Enregistrer devient actif",
          str(f.btn_enregistrer.cget('state')) == 'normal')
    f.essai_reussi = False
    f._boutons(True)
    check("apres un abandon, Enregistrer redevient inactif",
          str(f.btn_enregistrer.cget('state')) == 'disabled')

    check('les trois regions sont proposees avec un libelle accentue',
          sorted(module.REGIONS_FR) == ['jp', 'ous', 'us']
          and all('é' in t or t == 'Japon' for t in module.REGIONS_FR.values()))
    for code, libelle in module.REGIONS_FR.items():
        f.region.set(libelle)
        check('region "%s" -> %s' % (libelle, code), f.region_choisie() == code, f.region_choisie())
    f.region.set('texte inconnu')
    check("libelle inconnu : repli sur l'Europe", f.region_choisie() == 'ous')

    f.ssid.set('BoxSalon')
    f.mdp_wifi.set('mot-de-passe')
    f.user.set('a@exemple.fr')
    f.mdp_dex.set('secret-dexcom')
    f.journal.configure(state='normal')
    f.journal.delete('1.0', 'end')
    f.journal.configure(state='disabled')
    f.lire_etat()                      # ouvre un port simule : aucun materiel
    racine.update()
    journal = f.journal.get('1.0', 'end')
    check("aucun mot de passe dans la fenetre d'etat",
          'mot-de-passe' not in journal and 'secret-dexcom' not in journal)
    f.fermer()

    return rapport()


def rapport():
    largeur = max(len(n) for n, _, _ in resultats)
    echecs = 0
    for nom, ok, detail in resultats:
        print('   %-5s %-*s %s' % ('OK' if ok else 'ECHEC', largeur, nom, '' if ok else detail))
        echecs += 0 if ok else 1
    print('\ntest_pc_app : %d controles, %d echec(s)' % (len(resultats), echecs))
    return 0 if echecs == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
