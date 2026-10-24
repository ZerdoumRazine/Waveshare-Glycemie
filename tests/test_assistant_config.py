#!/usr/bin/env python3
# Simulates serial configuration and credential failures.
#
# These tests use simulations; they do not connect to a patient's device.
# Simulate USB setup to prove passwords never appear in logs and failed trials are reversible.
#
"""Controles de l'assistant de configuration par USB (tools/assistant_config.py).

Deux proprietes comptent plus que les autres :
  1. aucun mot de passe n'est affiche, ni ecrit dans un fichier ;
  2. des acces qui fonctionnent ne sont JAMAIS remplaces par des acces invalides : les nouveaux
     sont essayes, puis enregistres seulement apres reussite ET confirmation.

Les essais utilisent un appareil simule (aucune liaison serie, aucun materiel) : ils ne prouvent rien
sur le cable ni sur le firmware reel, seulement sur la logique de l'assistant.

Usage : python tests\\test_assistant_config.py
"""
import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SOURCE = os.path.join(ROOT, 'tools', 'assistant_config.py')
sys.path.insert(0, os.path.join(ROOT, 'tools'))

import assistant_config as ac  # noqa: E402  (the path must be added before the import)

SSID = 'BoxSalon'
MDP_WIFI = 'mot-de-passe-du-wifi-2024'
USER = 'antoine.dupont@exemple.fr'
MDP_DEX = 'MonMotDePasseDexcom!42'

ANCIEN_SSID = 'AncienneBox'
ANCIEN_MDP = 'ancien-mot-de-passe'
ANCIEN_USER = 'ancien@exemple.fr'
ANCIEN_MDP_DEX = 'AncienMotDePasse'

resultats = []


def check(nom, ok, detail=''):
    resultats.append((nom, bool(ok), detail))


class AppareilSimule(object):
    """Console du firmware simulee, avec la distinction essai / enregistre.

    enregistre_* : ce que l'appareil garderait apres une coupure de courant.
    essai_*      : valeurs en cours d'essai, perdues si elles ne sont pas confirmees.
    """

    def __init__(self, essais=None, muet=False, deja=None, effacement_echoue=False):
        self.recu = []
        self.sortie = b''
        self.tampon = b''
        self.essais = list(essais or [])    # etats successifs renvoyes par ident_etat
        self.muet = muet
        self.effacement_echoue = effacement_echoue
        self.ferme = False
        self.enregistre_wifi = deja[0] if deja else ('', '')
        self.enregistre_dexcom = deja[1] if deja else ('', '', '')
        self.essai_wifi = None
        self.essai_dexcom = None
        self.dernier_etat = (ac.ETAT_INACTIF, '')

    # --- transport ---
    def write(self, data):
        self.tampon += data
        while b'\n' in self.tampon:
            brut, self.tampon = self.tampon.split(b'\n', 1)
            cmd = brut.decode('utf-8').strip()
            self.recu.append(cmd)
            if not self.muet:
                self._commande(cmd)
        return len(data)

    def read(self, taille):
        morceau, self.sortie = self.sortie[:taille], self.sortie[taille:]
        return morceau

    def close(self):
        self.ferme = True

    # --- practical views for controls ---
    @property
    def essai_actif(self):
        return self.essai_wifi is not None or self.essai_dexcom is not None

    def effectif_wifi(self):
        return self.essai_wifi if self.essai_wifi is not None else self.enregistre_wifi

    # --- firmware simule ---
    def _repondre(self, ligne):
        self.sortie += (ligne + '\r\n').encode('utf-8')

    def _masque_compte(self):
        user = self.enregistre_dexcom[0]
        if not user:
            return '-'
        return '%c***%s' % (user[0], user[user.find('@'):]) if '@' in user else '%c***' % user[0]

    def _commande(self, cmd):
        if cmd == 'ident_info':
            self._repondre('I (1234) identifiants: Identifiants : Wi-Fi renseigne')  # bruit de journal
            self._repondre('IDENT wifi="%s" mdp_wifi=%s dexcom="%s" mdp_dexcom=%s region=%s '
                           'configure=%d demo=0 origine=saisie essai=%d'
                           % (self.enregistre_wifi[0] or '-',
                              ('........ (%d)' % len(self.enregistre_wifi[1]))
                              if self.enregistre_wifi[1] else '-',
                              self._masque_compte(),
                              ('........ (%d)' % len(self.enregistre_dexcom[1]))
                              if self.enregistre_dexcom[1] else '-',
                              self.enregistre_dexcom[2] or 'ous',
                              1 if self.enregistre_wifi[0] and self.enregistre_dexcom[0] else 0,
                              1 if self.essai_actif else 0))
        elif cmd.startswith('ident_wifi '):
            reste = cmd[len('ident_wifi '):]
            if '|' not in reste:
                self._repondre('IDENT usage : ident_wifi <reseau>|<mot de passe>')
            else:
                ssid, mdp = reste.split('|', 1)
                if not ssid or len(mdp) > 64:
                    self._repondre('IDENT wifi ESP_ERR_INVALID_ARG')
                else:
                    self.essai_wifi = (ssid, mdp)
                    self._repondre("IDENT wifi a l'essai")
        elif cmd.startswith('ident_dexcom '):
            morceaux = cmd[len('ident_dexcom '):].split('|')
            if len(morceaux) != 3 or not morceaux[0] or morceaux[2] not in ('ous', 'us', 'jp'):
                self._repondre('IDENT dexcom ESP_ERR_INVALID_ARG')
            else:
                self.essai_dexcom = tuple(morceaux)
                self._repondre("IDENT dexcom a l'essai")
        elif cmd == 'ident_test':
            self._repondre('IDENT essai de connexion lance (suivre les traces)')
        elif cmd == 'ident_etat':
            etat = self.essais.pop(0) if len(self.essais) > 1 else (
                self.essais[0] if self.essais else (ac.ETAT_INACTIF, ''))
            self.dernier_etat = etat
            self._repondre('IDENT essai=%d detail=%s' % etat)
        elif cmd.startswith('ident_enregistrer'):
            if not self.essai_actif:
                self._repondre('IDENT enregistrer : aucun essai en cours')
            elif self.dernier_etat[0] != ac.ETAT_REUSSI:
                self._repondre('IDENT enregistrer refuse : essai non reussi (%s). '
                               'Lancez ident_test.' % self.dernier_etat[1])
            else:
                if self.essai_wifi is not None:
                    self.enregistre_wifi = self.essai_wifi
                if self.essai_dexcom is not None:
                    self.enregistre_dexcom = self.essai_dexcom
                self.essai_wifi = self.essai_dexcom = None
                self._repondre('IDENT enregistrer ok (redemarrer pour appliquer)')
        elif cmd == 'ident_annuler':
            self.essai_wifi = self.essai_dexcom = None
            self._repondre('IDENT annule (acces precedents conserves)')
        elif cmd == 'ident_effacer':
            if self.effacement_echoue:
                self._repondre("IDENT efface ESP_FAIL")
            else:
                self.enregistre_wifi, self.enregistre_dexcom = ('', ''), ('', '', '')
                self.essai_wifi = self.essai_dexcom = None
                self._repondre("IDENT efface ok (redemarrer pour appliquer)")


class Journal(object):
    """Capture tout ce que l'assistant affiche, pour verifier qu'aucun secret n'y figure."""

    def __enter__(self):
        self.texte = io.StringIO()
        self.vrai, sys.stdout = sys.stdout, self.texte
        return self

    def __exit__(self, *_):
        sys.stdout = self.vrai

    @property
    def contenu(self):
        return self.texte.getvalue()


def reponses(*valeurs):
    """Remplace les questions posees a l'utilisateur par des reponses preparees."""
    suite = list(valeurs)

    def demande(texte, defaut=''):
        return suite.pop(0) if suite else defaut
    return demande


def mdp_fixes(*valeurs):
    """Remplace getpass : meme refus du caractere | que la vraie saisie."""
    suite = list(valeurs)

    def saisie(texte):
        valeur = suite.pop(0) if suite else ''
        if '|' in valeur:
            print("Le caractere | n'est pas accepte par cet assistant : utilisez l'assistant de l'ecran.")
            return None
        return valeur
    return saisie


ANCIENS = (((ANCIEN_SSID, ANCIEN_MDP)), (ANCIEN_USER, ANCIEN_MDP_DEX, 'ous'))


# Feed deterministic network states to the serial assistant without hardware.
def scenario(essais, mdp_wifi=MDP_WIFI, mdp_dex=MDP_DEX, ssid=SSID, user=USER, region='1',
             muet=False, confirmation='oui', deja=ANCIENS):
    """Configuration complete sur un appareil simule -> (code, appareil, texte affiche)."""
    simule = AppareilSimule(essais, muet=muet, deja=deja)
    app = ac.Appareil(simule)
    with Journal() as j:
        code = ac.configurer(app, demande=reponses(ssid, region, user, confirmation),
                             mot_de_passe=mdp_fixes(mdp_wifi, mdp_dex))
    return code, simule, j.contenu


# Check that rejected trial credentials never replace the recorded account.
def anciens_intacts(simule):
    return (simule.enregistre_wifi == (ANCIEN_SSID, ANCIEN_MDP)
            and simule.enregistre_dexcom == (ANCIEN_USER, ANCIEN_MDP_DEX, 'ous'))


# Exercise success, refusal, disconnect and erase-failure paths.
def main():
    # --- 1. Configuration complete, essayee puis confirmee -------------------------------------
    etapes = [(ac.ETAT_EN_COURS, 'wifi'), (ac.ETAT_EN_COURS, 'internet'),
              (ac.ETAT_EN_COURS, 'dexcom'), (ac.ETAT_REUSSI, 'ok')]
    code, simule, texte = scenario(etapes)
    check('configuration complete : code de retour 0', code == 0, 'code %s' % code)
    check('les nouveaux acces sont enregistres apres confirmation',
          simule.enregistre_wifi == (SSID, MDP_WIFI)
          and simule.enregistre_dexcom == (USER, MDP_DEX, 'ous'))
    check('un essai de connexion est demande avant tout enregistrement',
          simule.recu.index('ident_test') < simule.recu.index('ident_enregistrer'))
    check("la reussite est annoncee a l'utilisateur", 'Tout fonctionne' in texte)
    check("l'enregistrement est annonce", 'enregistre' in texte)
    check('le redemarrage necessaire est explique', 'edemarrez' in texte)
    check("plus aucun essai en attente a la fin", not simule.essai_actif)
    for etape in ('connexion au Wi-Fi', 'acces a Internet', 'compte Dexcom'):
        check('etape affichee pendant la verification : %s' % etape, etape in texte)

    # --- 2. No password displays ---------------------------------------------------------
    check("le mot de passe du Wi-Fi n'est jamais affiche", MDP_WIFI not in texte)
    check("le mot de passe Dexcom n'est jamais affiche", MDP_DEX not in texte)
    envoye = '\n'.join(simule.recu)
    check('le mot de passe Wi-Fi part une seule fois, dans ident_wifi',
          envoye.count(MDP_WIFI) == 1 and ('ident_wifi %s|%s' % (SSID, MDP_WIFI)) in envoye)
    check('le mot de passe Dexcom part une seule fois, dans ident_dexcom',
          envoye.count(MDP_DEX) == 1 and ('ident_dexcom %s|%s|ous' % (USER, MDP_DEX)) in envoye)

    # --- 3. Accesses that work are never lost -----------------------------------
    code, simule, texte = scenario([(ac.ETAT_ECHEC, 'dexcom_refuse')])
    check('essai refuse : les anciens acces restent enregistres', anciens_intacts(simule))
    check('essai refuse : annulation envoyee a l\'appareil', 'ident_annuler' in simule.recu)
    check('essai refuse : aucun enregistrement demande',
          not any(c.startswith('ident_enregistrer') for c in simule.recu))
    check('essai refuse : l\'utilisateur est rassure sur ses anciens acces',
          'conserv' in texte and code == 1)

    code, simule, texte = scenario(etapes, confirmation='non')
    check('confirmation refusee : rien n\'est enregistre', anciens_intacts(simule) and code == 1)
    check('confirmation refusee : annulation envoyee', 'ident_annuler' in simule.recu)
    check('confirmation refusee : plus aucun essai en attente', not simule.essai_actif)

    simule = AppareilSimule([(ac.ETAT_ECHEC, 'wifi')], deja=ANCIENS)
    app = ac.Appareil(simule)
    simule._commande('ident_wifi NouvelleBox|nouveau-mot-de-passe')
    with Journal():
        app.envoyer('ident_enregistrer')
        app.lire(0.2)
    check("l'appareil refuse d'enregistrer sans essai reussi", anciens_intacts(simule))
    simule.recu = []
    with Journal():
        code = ac.enregistrer(ac.Appareil(simule), demande=reponses('oui'))
    check("refus d'enregistrement : code 1 et anciens acces conserves",
          code == 1 and anciens_intacts(simule))

    code, simule, texte = scenario(etapes, muet=True)
    check('cable debranche en cours de route : anciens acces conserves', anciens_intacts(simule))

    # --- 4. Choix de la region -----------------------------------------------------------------
    for choix, attendu in (('1', 'ous'), ('2', 'us'), ('3', 'jp'), ('', 'ous'), ('9', 'ous')):
        _, simule, _ = scenario([(ac.ETAT_REUSSI, 'ok')], region=choix)
        check('region "%s" -> %s' % (choix or 'reponse vide', attendu),
              simule.enregistre_dexcom[2] == attendu, simule.enregistre_dexcom[2])
    check('les trois regions du firmware sont proposees',
          sorted(code for code, _ in ac.REGIONS.values()) == ['jp', 'ous', 'us'])

    # --- 5. Echecs : chaque cause est expliquee en clair ---------------------------------------
    for cle, attendu in (('wifi', '2,4 GHz'), ('internet', 'Internet'), ('dexcom_refuse', 'refuse'),
                         ('dexcom_bloque', 'bloque'), ('dexcom_reseau', 'injoignable'),
                         ('dexcom_vide', 'incomplet'), ('memoire', 'Memoire'),
                         ('dexcom_autre', 'inattendu')):
        code, simule, texte = scenario([(ac.ETAT_ECHEC, cle)])
        check('echec "%s" : code 1 et cause expliquee' % cle, code == 1 and attendu in texte,
              (texte.strip().splitlines() or [''])[-1])
        check('echec "%s" : anciens acces conserves, aucun mot de passe affiche' % cle,
              anciens_intacts(simule) and MDP_WIFI not in texte and MDP_DEX not in texte)
    code, _, texte = scenario([(ac.ETAT_ECHEC, 'cause_imprevue')])
    check('cause imprevue : signalee sans planter', code == 1 and 'cause_imprevue' in texte)
    code, simule, texte = scenario([(ac.ETAT_INACTIF, '')])
    check("aucun resultat d'essai : code 1, renvoi vers l'ecran, anciens acces conserves",
          code == 1 and "l'ecran" in texte and anciens_intacts(simule))

    # --- 6. Seizures refused before sending --------------------------------------------------
    mdp_barre = 'mot|de|passe'
    code, simule, texte = scenario([(ac.ETAT_REUSSI, 'ok')], mdp_wifi=mdp_barre)
    check('mot de passe contenant | : refuse sans rien envoyer',
          code == 1 and not any(c.startswith('ident_wifi') for c in simule.recu))
    check("le mot de passe refuse n'est pas affiche", mdp_barre not in texte)
    for champ, valeur in (('ssid', ''), ('ssid', 'Box|Salon'), ('user', ''), ('mdp_dex', '')):
        code, simule, _ = scenario([(ac.ETAT_REUSSI, 'ok')], **{champ: valeur})
        check('%s "%s" : refuse, anciens acces conserves' % (champ, valeur or 'vide'),
              code == 1 and anciens_intacts(simule))

    # --- 7. Reading the state: nothing changes ----------------------------------------------
    simule = AppareilSimule(deja=((SSID, MDP_WIFI), (USER, MDP_DEX, 'ous')))
    app = ac.Appareil(simule)
    etat = app.etat()
    check("l'etat est lu sur l'appareil", etat is not None and etat.startswith('IDENT wifi='), etat or '')
    check("une trace de journal n'est pas prise pour l'etat", etat is None or 'identifiants:' not in etat)
    check("lire l'etat n'ecrit rien sur l'appareil", simule.recu == ['ident_info'], ', '.join(simule.recu))
    check("l'etat ne contient aucun mot de passe",
          etat is not None and MDP_WIFI not in etat and MDP_DEX not in etat)
    check("l'etat indique si un essai est en cours", etat is not None and 'essai=' in etat)
    check('appareil eteint : etat inconnu, pas de plantage',
          ac.Appareil(AppareilSimule(muet=True)).etat() is None)

    # --- 8. Effacement : confirmation obligatoire ----------------------------------------------
    simule = AppareilSimule(deja=((SSID, MDP_WIFI), (USER, MDP_DEX, 'ous')))
    app = ac.Appareil(simule)
    with Journal() as j:
        code = ac.effacer(app, demande=reponses('non'))
    check('reponse "non" : rien efface', code == 1 and simule.enregistre_wifi == (SSID, MDP_WIFI)
          and 'ident_effacer' not in simule.recu)
    check('reponse "non" : dit clairement que rien n\'a ete efface', 'Rien efface' in j.contenu)
    with Journal() as j:
        code = ac.effacer(app, demande=reponses(''))
    check('reponse vide : rien efface (defaut prudent)',
          code == 1 and simule.enregistre_wifi == (SSID, MDP_WIFI))
    with Journal() as j:
        code = ac.effacer(app, demande=reponses('OUI'))
    check('reponse "oui" : effacement demande a l\'appareil',
          code == 0 and 'ident_effacer' in simule.recu and simule.enregistre_wifi == ('', ''))
    check('apres effacement : le redemarrage est explique', 'edemarrez' in j.contenu)
    check('apres effacement : le mode demonstration est rappele', 'emonstration' in j.contenu)

    simule = AppareilSimule(deja=((SSID, MDP_WIFI), (USER, MDP_DEX, 'ous')),
                            effacement_echoue=True)
    app = ac.Appareil(simule)
    with Journal() as j:
        code = ac.effacer(app, demande=reponses('oui'))
    check('echec NVS : le refus renvoie un code non nul', code != 0)
    check('echec NVS : anciens acces conserves',
          simule.enregistre_wifi == (SSID, MDP_WIFI) and
          simule.enregistre_dexcom == (USER, MDP_DEX, 'ous'))
    check('echec NVS : aucune instruction de redemarrage trompeuse',
          'Redemarrez l\'appareil' not in j.contenu and 'PAS' in j.contenu)

    # --- 9. Hygiene du script ------------------------------------------------------------------
    source = io.open(SOURCE, encoding='utf-8').read()
    check('mot de passe demande sans echo (getpass)', 'getpass.getpass' in source)
    check("le script n'ouvre aucun fichier en ecriture", "'w'" not in source and '"w"' not in source)
    check("aucune adresse Internet : rien ne sort du cable",
          'http://' not in source and 'https://' not in source)
    check('aucun identifiant en dur dans le script',
          '@gmail' not in source and 'dexcom.com' not in source and 'razine' not in source.lower())
    check("le port est ouvert sans redemarrer l'appareil (DTR et RTS inactifs)",
          'dtr = False' in source and 'rts = False' in source and '0x0001' in source)
    check('aucune installation necessaire sous Windows (API Windows en secours)',
          'PortWin32' in source and 'ctypes' in source)
    check('les etats lus correspondent a gly_test_t du firmware',
          (ac.ETAT_INACTIF, ac.ETAT_EN_COURS, ac.ETAT_REUSSI, ac.ETAT_ECHEC) == (0, 1, 2, 3))

    largeur = max(len(n) for n, _, _ in resultats)
    echecs = 0
    for nom, ok, detail in resultats:
        print('   %-5s %-*s %s' % ('OK' if ok else 'ECHEC', largeur, nom, '' if ok else detail))
        echecs += 0 if ok else 1
    print('\ntest_assistant_config : %d controles, %d echec(s)' % (len(resultats), echecs))
    return 0 if echecs == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
