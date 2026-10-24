# Displays safe demo values for UI screenshots.
#
# Design note: Demo screenshots must never reveal real user details.
#!/usr/bin/env python3
# Launches a demonstration of the Windows configuration UI without a real device.
#
"""Launch the setup application with fictional demonstration data for safe screenshots.

No personal Dexcom account, Wi-Fi password or patient's measurement should
appear in documentation or promotional material."""
import importlib.util
import os
import sys
import tkinter as tk

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ICI, '..'))

spec = importlib.util.spec_from_file_location('glycemie_connexion',
                                              os.path.join(ICI, 'glycemie_connexion.pyw'))
app = importlib.util.module_from_spec(spec)
spec.loader.exec_module(app)

PORTS_FICTIFS = [('COM7', 'USB-Enhanced-SERIAL CH343 (WaveShare)'), ('COM3', 'Port série')]


# Populate the window with examples for screenshots instead of live Dexcom data.
def main():
    app.ac.lister_ports = lambda: list(PORTS_FICTIFS)
    app.ac.trouver_port = lambda: 'COM7'
    app.ecran_haute_definition()
    racine = tk.Tk()
    f = app.Fenetre(racine)
    f.ssid.set('MaBox-Salon')
    f.mdp_wifi.set('exemple-de-mot-de-passe')
    f.user.set('marie@exemple.fr')
    f.mdp_dex.set('ExempleMotDePasse')
    f.dire("Appareil proposé : COM7 — USB-Enhanced-SERIAL CH343 (WaveShare)")
    f.dire('')
    f.dire('--- Essai des accès ---')
    f.dire('Envoi du réseau Wi-Fi…')
    f.dire('Envoi du compte Dexcom…')
    f.dire("Essai de connexion (Wi-Fi, Internet, Dexcom) : jusqu'à une minute…")
    f.dire('   … connexion au Wi-Fi')
    f.dire("   … accès à Internet et mise à l'heure")
    f.dire('   … connexion au compte Dexcom')
    f.dire('Tout fonctionne. Appuyez sur « Enregistrer » pour garder ces accès dans '
           "l'écran, ou sur « Annuler l'essai » pour revenir aux précédents.")
    f.essai_reussi = True
    f._boutons(True)
    f.etiquette_etat.configure(text='Réseau : MaBox-Salon   |   Compte Dexcom : m***@exemple.fr '
                                    '  |   configuré')
    racine.after(25000, f.fermer)   # closes alone if the catch has not taken place
    racine.mainloop()
    return 0


if __name__ == '__main__':
    sys.exit(main())
