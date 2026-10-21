#!/usr/bin/env python3
# EN: Defines fixed spoken phrases and grammar-aware number words in six languages.
# FR: Contient les phrases fixes et les nombres prononcés dans six langues.
"""Textes de la banque vocale, langue par langue (francais, anglais, espagnol, allemand, italien, portugais).

Utilise par gen_voice_google.py. Chaque langue fournit :
  - la voix Google Cloud a employer (meme timbre partout : Chirp 3 HD "Aoede") ;
  - les 35 phrases fixes (alertes, rappels des consommables, tendances, reponses vocales) ;
  - les nombres en lettres de 40 a 400, pour les 361 fichiers "glucose_NNN.wav".

Le portugais est genere avec la voix pt-BR : Google ne propose pas de voix Chirp 3 HD en pt-PT.

Les phrases reprennent la formulation du francais d'origine. Deux habitudes sont conservees :
  - "cathetere" en francais, parce que la voix avalait le R final de "catheter" (constat de l'utilisateur) ;
  - les nombres sont ecrits en lettres plutot qu'en chiffres, pour que la prononciation soit stable.
"""

# --------------------------------------------------------------------------------------------------
# Nombres en lettres, de 40 a 400
# --------------------------------------------------------------------------------------------------

_FR_UNITS = ['zéro', 'un', 'deux', 'trois', 'quatre', 'cinq', 'six', 'sept', 'huit', 'neuf', 'dix', 'onze',
             'douze', 'treize', 'quatorze', 'quinze', 'seize']
_FR_TENS = {2: 'vingt', 3: 'trente', 4: 'quarante', 5: 'cinquante', 6: 'soixante'}


def _fr_below_100(n):
    if n <= 16:
        return _FR_UNITS[n]
    if n < 20:
        return 'dix-' + _FR_UNITS[n - 10]
    if n < 70:
        tens, unit = divmod(n, 10)
        if unit == 0:
            return _FR_TENS[tens]
        return _FR_TENS[tens] + (' et un' if unit == 1 else '-' + _FR_UNITS[unit])
    if n < 80:
        return 'soixante et onze' if n == 71 else 'soixante-' + _fr_below_100(n - 60)
    return 'quatre-vingts' if n == 80 else 'quatre-vingt-' + _fr_below_100(n - 80)


# Spell a French number with the language-specific tens and hundreds rules.
def number_fr(n):
    """Orthographe traditionnelle : soixante-dix-huit, deux cents, cent un."""
    if n < 100:
        return _fr_below_100(n)
    hundreds, rest = divmod(n, 100)
    head = 'cent' if hundreds == 1 else _FR_UNITS[hundreds] + (' cents' if rest == 0 else ' cent')
    return head if rest == 0 else head + ' ' + _fr_below_100(rest)


_EN_UNITS = ['zero', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten', 'eleven',
             'twelve', 'thirteen', 'fourteen', 'fifteen', 'sixteen', 'seventeen', 'eighteen', 'nineteen']
_EN_TENS = {2: 'twenty', 3: 'thirty', 4: 'forty', 5: 'fifty', 6: 'sixty', 7: 'seventy', 8: 'eighty', 9: 'ninety'}


def _en_below_100(n):
    if n < 20:
        return _EN_UNITS[n]
    tens, unit = divmod(n, 10)
    return _EN_TENS[tens] if unit == 0 else _EN_TENS[tens] + '-' + _EN_UNITS[unit]


# Express number words in US English for the matching voice bank.
def number_en(n):
    """Usage courant pour une mesure : one hundred twenty, two hundred, four hundred."""
    if n < 100:
        return _en_below_100(n)
    hundreds, rest = divmod(n, 100)
    head = _EN_UNITS[hundreds] + ' hundred'
    return head if rest == 0 else head + ' ' + _en_below_100(rest)


_ES_UNITS = ['cero', 'uno', 'dos', 'tres', 'cuatro', 'cinco', 'seis', 'siete', 'ocho', 'nueve', 'diez', 'once',
             'doce', 'trece', 'catorce', 'quince', 'dieciséis', 'diecisiete', 'dieciocho', 'diecinueve',
             'veinte', 'veintiuno', 'veintidós', 'veintitrés', 'veinticuatro', 'veinticinco', 'veintiséis',
             'veintisiete', 'veintiocho', 'veintinueve']
_ES_TENS = {3: 'treinta', 4: 'cuarenta', 5: 'cincuenta', 6: 'sesenta', 7: 'setenta', 8: 'ochenta', 9: 'noventa'}
_ES_HUNDREDS = {1: 'ciento', 2: 'doscientos', 3: 'trescientos', 4: 'cuatrocientos'}


def _es_below_100(n):
    if n < 30:
        return _ES_UNITS[n]
    tens, unit = divmod(n, 10)
    return _ES_TENS[tens] if unit == 0 else _ES_TENS[tens] + ' y ' + _ES_UNITS[unit]


# Handle Spanish hundreds and the cien/ciento distinction.
def number_es(n):
    """cien pour 100 tout seul, ciento veinte, doscientos, cuatrocientos."""
    if n < 100:
        return _es_below_100(n)
    hundreds, rest = divmod(n, 100)
    if rest == 0:
        return 'cien' if hundreds == 1 else _ES_HUNDREDS[hundreds]
    return _ES_HUNDREDS[hundreds] + ' ' + _es_below_100(rest)


_DE_UNITS = ['null', 'eins', 'zwei', 'drei', 'vier', 'fünf', 'sechs', 'sieben', 'acht', 'neun', 'zehn', 'elf',
             'zwölf', 'dreizehn', 'vierzehn', 'fünfzehn', 'sechzehn', 'siebzehn', 'achtzehn', 'neunzehn']
_DE_TENS = {2: 'zwanzig', 3: 'dreißig', 4: 'vierzig', 5: 'fünfzig', 6: 'sechzig', 7: 'siebzig', 8: 'achtzig',
            9: 'neunzig'}


def _de_below_100(n):
    if n < 20:
        return _DE_UNITS[n]
    tens, unit = divmod(n, 10)
    if unit == 0:
        return _DE_TENS[tens]
    # einundvierzig, zweiundvierzig... ("eins" devient "ein" en composition)
    return ('ein' if unit == 1 else _DE_UNITS[unit]) + 'und' + _DE_TENS[tens]


# Use German unit-before-ten forms when building compound numbers.
def number_de(n):
    """einhundertzwanzig, zweihundert, vierhundert ; un seul mot, comme en allemand courant."""
    if n < 100:
        return _de_below_100(n)
    hundreds, rest = divmod(n, 100)
    head = ('ein' if hundreds == 1 else _DE_UNITS[hundreds]) + 'hundert'
    return head if rest == 0 else head + _de_below_100(rest)


_IT_UNITS = ['zero', 'uno', 'due', 'tre', 'quattro', 'cinque', 'sei', 'sette', 'otto', 'nove', 'dieci', 'undici',
             'dodici', 'tredici', 'quattordici', 'quindici', 'sedici', 'diciassette', 'diciotto', 'diciannove']
_IT_TENS = {2: 'venti', 3: 'trenta', 4: 'quaranta', 5: 'cinquanta', 6: 'sessanta', 7: 'settanta', 8: 'ottanta',
            9: 'novanta'}
_IT_HUNDREDS = {1: 'cento', 2: 'duecento', 3: 'trecento', 4: 'quattrocento'}


def _it_below_100(n):
    if n < 20:
        return _IT_UNITS[n]
    tens, unit = divmod(n, 10)
    if unit == 0:
        return _IT_TENS[tens]
    base = _IT_TENS[tens]
    if unit in (1, 8):
        base = base[:-1]  # elision : quaranta + uno -> quarantuno, quaranta + otto -> quarantotto
    return base + _IT_UNITS[unit]


# Handle Italian elisions around compound numbers.
def number_it(n):
    """centoventi, duecento, quattrocento ; un seul mot."""
    if n < 100:
        return _it_below_100(n)
    hundreds, rest = divmod(n, 100)
    head = _IT_HUNDREDS[hundreds]
    if rest == 0:
        return head
    tail = _it_below_100(rest)
    if tail[0] in 'ou':
        head = head[:-1]  # cento + otto -> centotto
    return head + tail


_PT_UNITS = ['zero', 'um', 'dois', 'três', 'quatro', 'cinco', 'seis', 'sete', 'oito', 'nove', 'dez', 'onze',
             'doze', 'treze', 'catorze', 'quinze', 'dezasseis', 'dezassete', 'dezoito', 'dezanove']
_PT_TENS = {2: 'vinte', 3: 'trinta', 4: 'quarenta', 5: 'cinquenta', 6: 'sessenta', 7: 'setenta', 8: 'oitenta',
            9: 'noventa'}
_PT_HUNDREDS = {1: 'cento', 2: 'duzentos', 3: 'trezentos', 4: 'quatrocentos'}


def _pt_below_100(n):
    if n < 20:
        return _PT_UNITS[n]
    tens, unit = divmod(n, 10)
    return _PT_TENS[tens] if unit == 0 else _PT_TENS[tens] + ' e ' + _PT_UNITS[unit]


# Use Brazilian Portuguese wording for the installed Portuguese voice.
def number_pt(n):
    """cem tout seul, cento e vinte, duzentos, quatrocentos."""
    if n < 100:
        return _pt_below_100(n)
    hundreds, rest = divmod(n, 100)
    if rest == 0:
        return 'cem' if hundreds == 1 else _PT_HUNDREDS[hundreds]
    return _PT_HUNDREDS[hundreds] + ' e ' + _pt_below_100(rest)


# --------------------------------------------------------------------------------------------------
# Phrases fixes
# --------------------------------------------------------------------------------------------------
# Ordre identique dans toutes les langues ; les noms de fichiers ne changent jamais.

ORDRE = [
    'alert_urgent_low', 'alert_low', 'alert_high', 'alert_urgent_high', 'alert_no_data',
    'sensor_48h', 'sensor_24h', 'sensor_12h', 'sensor_grace', 'sensor_2h',
    'glucose_low', 'glucose_high',
    'catheter_24h', 'catheter_6h', 'catheter_2h', 'catheter_due',
    'trend_flat', 'trend_up_slow', 'trend_up', 'trend_up_fast',
    'trend_down_slow', 'trend_down', 'trend_down_fast',
    'vc_data_old', 'vc_no_data', 'vc_sensor_confirm', 'vc_sensor_saved',
    'vc_catheter_confirm', 'vc_catheter_saved', 'vc_cancelled', 'vc_time_unavailable',
    'sensor_rappel_24h', 'sensor_rappel_2h', 'catheter_rappel_2h', 'vc_sensor_ask',
]

FIXES = {
    'fr': {
        'alert_urgent_low': 'Attention, glycémie très basse.',
        'alert_low': 'Attention, glycémie basse.',
        'alert_high': 'Attention, glycémie élevée.',
        'alert_urgent_high': 'Attention, glycémie très élevée.',
        'alert_no_data': 'Aucune nouvelle donnée.',
        'sensor_48h': 'Le capteur devra être remplacé dans deux jours.',
        'sensor_24h': 'Le capteur doit être remplacé demain.',
        'sensor_12h': 'Le capteur doit être remplacé dans douze heures.',
        'sensor_grace': 'Le capteur est maintenant en période de grâce.',
        'sensor_2h': 'Le capteur devra être remplacé dans deux heures.',
        'glucose_low': 'Glycémie inférieure à quarante.',
        'glucose_high': 'Glycémie supérieure à quatre cents.',
        'catheter_24h': 'Le cathétère doit être changé dans vingt-quatre heures.',
        'catheter_6h': 'Le cathétère doit être changé dans six heures.',
        'catheter_2h': 'Le cathétère doit être changé dans deux heures.',
        'catheter_due': 'Le cathétère doit être changé maintenant.',
        'trend_flat': 'Stable.',
        'trend_up_slow': 'En légère hausse.',
        'trend_up': 'En hausse.',
        'trend_up_fast': 'En hausse rapide.',
        'trend_down_slow': 'En légère baisse.',
        'trend_down': 'En baisse.',
        'trend_down_fast': 'En baisse rapide.',
        'vc_data_old': 'Données anciennes. Dernière mesure connue.',
        'vc_no_data': 'Aucune donnée de glycémie disponible.',
        'vc_sensor_confirm': 'Nouveau capteur. Répétez pour confirmer.',
        'vc_sensor_saved': 'Nouveau capteur enregistré.',
        'vc_catheter_confirm': 'Nouveau cathétère. Répétez pour confirmer.',
        'vc_catheter_saved': 'Nouveau cathétère enregistré.',
        'vc_cancelled': 'Commande annulée.',
        'vc_time_unavailable': 'Heure non synchronisée. Enregistrement impossible.',
        'sensor_rappel_24h': 'Rappel. Le capteur doit être changé demain.',
        'sensor_rappel_2h': 'Rappel. Le capteur doit être changé dans deux heures.',
        'catheter_rappel_2h': 'Rappel. Le cathétère doit être changé dans deux heures.',
        'vc_sensor_ask': 'Nouveau capteur. Dites confirmer capteur.',
    },
    'en': {
        'alert_urgent_low': 'Warning, glucose very low.',
        'alert_low': 'Warning, glucose low.',
        'alert_high': 'Warning, glucose high.',
        'alert_urgent_high': 'Warning, glucose very high.',
        'alert_no_data': 'No new data.',
        'sensor_48h': 'The sensor will need replacing in two days.',
        'sensor_24h': 'The sensor must be replaced tomorrow.',
        'sensor_12h': 'The sensor must be replaced in twelve hours.',
        'sensor_grace': 'The sensor is now in its grace period.',
        'sensor_2h': 'The sensor will need replacing in two hours.',
        'glucose_low': 'Glucose below forty.',
        'glucose_high': 'Glucose above four hundred.',
        'catheter_24h': 'The catheter must be changed in twenty-four hours.',
        'catheter_6h': 'The catheter must be changed in six hours.',
        'catheter_2h': 'The catheter must be changed in two hours.',
        'catheter_due': 'The catheter must be changed now.',
        'trend_flat': 'Steady.',
        'trend_up_slow': 'Rising slowly.',
        'trend_up': 'Rising.',
        'trend_up_fast': 'Rising fast.',
        'trend_down_slow': 'Falling slowly.',
        'trend_down': 'Falling.',
        'trend_down_fast': 'Falling fast.',
        'vc_data_old': 'Old data. Last known reading.',
        'vc_no_data': 'No glucose reading available.',
        'vc_sensor_confirm': 'New sensor. Repeat to confirm.',
        'vc_sensor_saved': 'New sensor recorded.',
        'vc_catheter_confirm': 'New catheter. Repeat to confirm.',
        'vc_catheter_saved': 'New catheter recorded.',
        'vc_cancelled': 'Command cancelled.',
        'vc_time_unavailable': 'Clock not synchronised. Cannot record.',
        'sensor_rappel_24h': 'Reminder. The sensor must be changed tomorrow.',
        'sensor_rappel_2h': 'Reminder. The sensor must be changed in two hours.',
        'catheter_rappel_2h': 'Reminder. The catheter must be changed in two hours.',
        'vc_sensor_ask': 'New sensor. Say confirmer capteur.',
    },
    'es': {
        'alert_urgent_low': 'Atención, glucosa muy baja.',
        'alert_low': 'Atención, glucosa baja.',
        'alert_high': 'Atención, glucosa alta.',
        'alert_urgent_high': 'Atención, glucosa muy alta.',
        'alert_no_data': 'Sin datos nuevos.',
        'sensor_48h': 'El sensor deberá cambiarse dentro de dos días.',
        'sensor_24h': 'El sensor debe cambiarse mañana.',
        'sensor_12h': 'El sensor debe cambiarse dentro de doce horas.',
        'sensor_grace': 'El sensor está ahora en período de gracia.',
        'sensor_2h': 'El sensor deberá cambiarse dentro de dos horas.',
        'glucose_low': 'Glucosa por debajo de cuarenta.',
        'glucose_high': 'Glucosa por encima de cuatrocientos.',
        'catheter_24h': 'El catéter debe cambiarse dentro de veinticuatro horas.',
        'catheter_6h': 'El catéter debe cambiarse dentro de seis horas.',
        'catheter_2h': 'El catéter debe cambiarse dentro de dos horas.',
        'catheter_due': 'El catéter debe cambiarse ahora.',
        'trend_flat': 'Estable.',
        'trend_up_slow': 'Subiendo despacio.',
        'trend_up': 'Subiendo.',
        'trend_up_fast': 'Subiendo rápido.',
        'trend_down_slow': 'Bajando despacio.',
        'trend_down': 'Bajando.',
        'trend_down_fast': 'Bajando rápido.',
        'vc_data_old': 'Datos antiguos. Última lectura conocida.',
        'vc_no_data': 'No hay lecturas de glucosa disponibles.',
        'vc_sensor_confirm': 'Nuevo sensor. Repita para confirmar.',
        'vc_sensor_saved': 'Nuevo sensor registrado.',
        'vc_catheter_confirm': 'Nuevo catéter. Repita para confirmar.',
        'vc_catheter_saved': 'Nuevo catéter registrado.',
        'vc_cancelled': 'Comando cancelado.',
        'vc_time_unavailable': 'Hora no sincronizada. No se puede registrar.',
        'sensor_rappel_24h': 'Recordatorio. El sensor debe cambiarse mañana.',
        'sensor_rappel_2h': 'Recordatorio. El sensor debe cambiarse dentro de dos horas.',
        'catheter_rappel_2h': 'Recordatorio. El catéter debe cambiarse dentro de dos horas.',
        'vc_sensor_ask': 'Nuevo sensor. Diga confirmer capteur.',
    },
    'de': {
        'alert_urgent_low': 'Achtung, Glukose sehr niedrig.',
        'alert_low': 'Achtung, Glukose niedrig.',
        'alert_high': 'Achtung, Glukose hoch.',
        'alert_urgent_high': 'Achtung, Glukose sehr hoch.',
        'alert_no_data': 'Keine neuen Daten.',
        'sensor_48h': 'Der Sensor muss in zwei Tagen gewechselt werden.',
        'sensor_24h': 'Der Sensor muss morgen gewechselt werden.',
        'sensor_12h': 'Der Sensor muss in zwölf Stunden gewechselt werden.',
        'sensor_grace': 'Der Sensor ist jetzt in der Nachfrist.',
        'sensor_2h': 'Der Sensor muss in zwei Stunden gewechselt werden.',
        'glucose_low': 'Glukose unter vierzig.',
        'glucose_high': 'Glukose über vierhundert.',
        'catheter_24h': 'Der Katheter muss in vierundzwanzig Stunden gewechselt werden.',
        'catheter_6h': 'Der Katheter muss in sechs Stunden gewechselt werden.',
        'catheter_2h': 'Der Katheter muss in zwei Stunden gewechselt werden.',
        'catheter_due': 'Der Katheter muss jetzt gewechselt werden.',
        'trend_flat': 'Stabil.',
        'trend_up_slow': 'Steigt langsam.',
        'trend_up': 'Steigt.',
        'trend_up_fast': 'Steigt schnell.',
        'trend_down_slow': 'Fällt langsam.',
        'trend_down': 'Fällt.',
        'trend_down_fast': 'Fällt schnell.',
        'vc_data_old': 'Alte Daten. Letzter bekannter Messwert.',
        'vc_no_data': 'Kein Glukosewert verfügbar.',
        'vc_sensor_confirm': 'Neuer Sensor. Zum Bestätigen wiederholen.',
        'vc_sensor_saved': 'Neuer Sensor gespeichert.',
        'vc_catheter_confirm': 'Neuer Katheter. Zum Bestätigen wiederholen.',
        'vc_catheter_saved': 'Neuer Katheter gespeichert.',
        'vc_cancelled': 'Befehl abgebrochen.',
        'vc_time_unavailable': 'Uhrzeit nicht synchronisiert. Speichern nicht möglich.',
        'sensor_rappel_24h': 'Erinnerung. Der Sensor muss morgen gewechselt werden.',
        'sensor_rappel_2h': 'Erinnerung. Der Sensor muss in zwei Stunden gewechselt werden.',
        'catheter_rappel_2h': 'Erinnerung. Der Katheter muss in zwei Stunden gewechselt werden.',
        'vc_sensor_ask': 'Neuer Sensor. Sagen Sie confirmer capteur.',
    },
    'it': {
        'alert_urgent_low': 'Attenzione, glicemia molto bassa.',
        'alert_low': 'Attenzione, glicemia bassa.',
        'alert_high': 'Attenzione, glicemia alta.',
        'alert_urgent_high': 'Attenzione, glicemia molto alta.',
        'alert_no_data': 'Nessun dato nuovo.',
        'sensor_48h': 'Il sensore dovrà essere sostituito tra due giorni.',
        'sensor_24h': 'Il sensore deve essere sostituito domani.',
        'sensor_12h': 'Il sensore deve essere sostituito tra dodici ore.',
        'sensor_grace': 'Il sensore è ora nel periodo di tolleranza.',
        'sensor_2h': 'Il sensore dovrà essere sostituito tra due ore.',
        'glucose_low': 'Glicemia sotto quaranta.',
        'glucose_high': 'Glicemia sopra quattrocento.',
        'catheter_24h': 'Il catetere deve essere sostituito tra ventiquattro ore.',
        'catheter_6h': 'Il catetere deve essere sostituito tra sei ore.',
        'catheter_2h': 'Il catetere deve essere sostituito tra due ore.',
        'catheter_due': 'Il catetere deve essere sostituito adesso.',
        'trend_flat': 'Stabile.',
        'trend_up_slow': 'In lieve aumento.',
        'trend_up': 'In aumento.',
        'trend_up_fast': 'In rapido aumento.',
        'trend_down_slow': 'In lieve calo.',
        'trend_down': 'In calo.',
        'trend_down_fast': 'In rapido calo.',
        'vc_data_old': 'Dati vecchi. Ultima misura nota.',
        'vc_no_data': 'Nessuna misura di glicemia disponibile.',
        'vc_sensor_confirm': 'Nuovo sensore. Ripeti per confermare.',
        'vc_sensor_saved': 'Nuovo sensore registrato.',
        'vc_catheter_confirm': 'Nuovo catetere. Ripeti per confermare.',
        'vc_catheter_saved': 'Nuovo catetere registrato.',
        'vc_cancelled': 'Comando annullato.',
        'vc_time_unavailable': 'Ora non sincronizzata. Registrazione impossibile.',
        'sensor_rappel_24h': 'Promemoria. Il sensore deve essere sostituito domani.',
        'sensor_rappel_2h': 'Promemoria. Il sensore deve essere sostituito tra due ore.',
        'catheter_rappel_2h': 'Promemoria. Il catetere deve essere sostituito tra due ore.',
        'vc_sensor_ask': 'Nuovo sensore. Di confirmer capteur.',
    },
    'pt': {
        'alert_urgent_low': 'Atenção, glicemia muito baixa.',
        'alert_low': 'Atenção, glicemia baixa.',
        'alert_high': 'Atenção, glicemia alta.',
        'alert_urgent_high': 'Atenção, glicemia muito alta.',
        'alert_no_data': 'Sem dados novos.',
        'sensor_48h': 'O sensor terá de ser substituído dentro de dois dias.',
        'sensor_24h': 'O sensor deve ser substituído amanhã.',
        'sensor_12h': 'O sensor deve ser substituído dentro de doze horas.',
        'sensor_grace': 'O sensor está agora no período de tolerância.',
        'sensor_2h': 'O sensor terá de ser substituído dentro de duas horas.',
        'glucose_low': 'Glicemia abaixo de quarenta.',
        'glucose_high': 'Glicemia acima de quatrocentos.',
        'catheter_24h': 'O cateter deve ser mudado dentro de vinte e quatro horas.',
        'catheter_6h': 'O cateter deve ser mudado dentro de seis horas.',
        'catheter_2h': 'O cateter deve ser mudado dentro de duas horas.',
        'catheter_due': 'O cateter deve ser mudado agora.',
        'trend_flat': 'Estável.',
        'trend_up_slow': 'A subir devagar.',
        'trend_up': 'A subir.',
        'trend_up_fast': 'A subir depressa.',
        'trend_down_slow': 'A descer devagar.',
        'trend_down': 'A descer.',
        'trend_down_fast': 'A descer depressa.',
        'vc_data_old': 'Dados antigos. Última leitura conhecida.',
        'vc_no_data': 'Nenhuma leitura de glicemia disponível.',
        'vc_sensor_confirm': 'Novo sensor. Repita para confirmar.',
        'vc_sensor_saved': 'Novo sensor registado.',
        'vc_catheter_confirm': 'Novo cateter. Repita para confirmar.',
        'vc_catheter_saved': 'Novo cateter registado.',
        'vc_cancelled': 'Comando cancelado.',
        'vc_time_unavailable': 'Hora não sincronizada. Não é possível registar.',
        'sensor_rappel_24h': 'Lembrete. O sensor deve ser substituído amanhã.',
        'sensor_rappel_2h': 'Lembrete. O sensor deve ser substituído dentro de duas horas.',
        'catheter_rappel_2h': 'Lembrete. O cateter deve ser mudado dentro de duas horas.',
        'vc_sensor_ask': 'Novo sensor. Diga confirmer capteur.',
    },
}

# "Glycemie <valeur>." : le mot qui precede le nombre, par langue.
GLUCOSE_PREFIX = {
    'fr': 'Glycémie %s.',
    'en': 'Glucose %s.',
    'es': 'Glucosa %s.',
    'de': 'Glukose %s.',
    'it': 'Glicemia %s.',
    'pt': 'Glicemia %s.',
}

NOMBRE = {'fr': number_fr, 'en': number_en, 'es': number_es, 'de': number_de, 'it': number_it, 'pt': number_pt}

# Meme timbre dans toutes les langues. Le portugais est en pt-BR : Google ne propose pas de Chirp 3 HD en pt-PT.
VOIX = {
    'fr': ('fr-FR', 'fr-FR-Chirp3-HD-Aoede'),
    'en': ('en-US', 'en-US-Chirp3-HD-Aoede'),
    'es': ('es-ES', 'es-ES-Chirp3-HD-Aoede'),
    'de': ('de-DE', 'de-DE-Chirp3-HD-Aoede'),
    'it': ('it-IT', 'it-IT-Chirp3-HD-Aoede'),
    'pt': ('pt-BR', 'pt-BR-Chirp3-HD-Aoede'),
}

LANGUES = list(VOIX)

GLUCOSE_MIN = 40
GLUCOSE_MAX = 400


# Return stable clip filenames and spoken text for a single language.
def phrases(lang):
    """[(nom de fichier, texte)] dans l'ordre de generation, pour la langue demandee."""
    if lang not in FIXES:
        raise ValueError('langue inconnue : %s (%s)' % (lang, ', '.join(LANGUES)))
    textes = FIXES[lang]
    manquants = [k for k in ORDRE if k not in textes]
    if manquants:
        raise ValueError('phrases manquantes en %s : %s' % (lang, ', '.join(manquants)))
    items = [(k + '.wav', textes[k]) for k in ORDRE]
    mot = NOMBRE[lang]
    gabarit = GLUCOSE_PREFIX[lang]
    items += [('glucose_%03d.wav' % v, gabarit % mot(v)) for v in range(GLUCOSE_MIN, GLUCOSE_MAX + 1)]
    return items


if __name__ == '__main__':
    import sys
    for code in (sys.argv[1:] or LANGUES):
        items = phrases(code)
        chars = sum(len(t) for _, t in items)
        print('=== %s : %d phrases, %d caracteres, voix %s' % (code, len(items), chars, VOIX[code][1]))
        for nom in ('alert_low.wav', 'glucose_040.wav', 'glucose_078.wav', 'glucose_100.wav',
                    'glucose_121.wav', 'glucose_180.wav', 'glucose_208.wav', 'glucose_400.wav'):
            texte = dict(items)[nom]
            print('    %-18s %s' % (nom, texte))
