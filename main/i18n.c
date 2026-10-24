/*
 * i18n.c: Six-language interface text catalog.
 * Design note: String IDs must remain stable even when translations change.
 */

/*
 * Maps stable text IDs to French, English, Spanish, German, Italian and Portuguese.
 * Associates text identifiers with the six languages of the interface.
 */
#include "i18n.h"

#include <stdio.h>
#include <string.h>

/* Langue courante : lue depuis la NVS at the start (settings.c), French before that. */
static lang_t s_lang = LANG_FR;

lang_t i18n_lang(void)
{
    return s_lang;
}

/* Apply the chosen catalog immediately while leaving glucose values unchanged. */
void i18n_set_lang(lang_t lang)
{
    if (lang >= 0 && lang < LANG_COUNT) {
        s_lang = lang;
    }
}

static const char *const k_lang_names[LANG_COUNT] = {
    "Français", "English", "Español", "Deutsch", "Italiano", "Português",
};

static const char *const k_lang_codes[LANG_COUNT] = {"fr", "en", "es", "de", "it", "pt"};

const char *lang_name(lang_t lang)
{
    return (lang >= 0 && lang < LANG_COUNT) ? k_lang_names[lang] : k_lang_names[LANG_FR];
}

const char *lang_code(lang_t lang)
{
    return (lang >= 0 && lang < LANG_COUNT) ? k_lang_codes[lang] : k_lang_codes[LANG_FR];
}

/* ------------------------------------------------------------------------------------------------
 * Translation table: one line per text, in the order of str_id_t.
 * Order of columns: French, English, Spanish, German, Italian, Portuguese.
 * The format specifiers (%d, %s, %%) must be identical and in the same order
 * sur toute la ligne : i18n_check() le verifie au demarrage.
 * ---------------------------------------------------------------------------------------------- */
static const char *const k_tr[S_COUNT][LANG_COUNT] = {
[S_NAV_HOME] = {"ACCUEIL", "HOME", "INICIO", "START", "HOME", "INÍCIO"},
[S_NAV_GRAPH] = {"GRAPHIQUE", "CHART", "GRÁFICO", "VERLAUF", "GRAFICO", "GRÁFICO"},
[S_NAV_TODAY] = {"AUJOURD'HUI", "TODAY", "HOY", "HEUTE", "OGGI", "HOJE"},
[S_NAV_CONSUM] = {"CONSOMMABLES", "SUPPLIES", "FUNGIBLES", "VERBRAUCH", "MATERIALI", "CONSUMÍVEIS"},
[S_NAV_SETTINGS] = {"RÉGLAGES", "SETTINGS", "AJUSTES", "EINSTELLUNG", "IMPOSTAZIONI", "DEFINIÇÕES"},
[S_DEMO_MODE] = {"MODE DÉMO", "DEMO MODE", "MODO DEMO", "DEMO-MODUS", "MODO DEMO", "MODO DEMO"},
[S_TIME_NOT_SYNCED] = {"Heure non synchronisée", "Clock not synchronised", "Hora no sincronizada",
                             "Uhrzeit nicht synchronisiert", "Ora non sincronizzata", "Hora não sincronizada"},

[S_FRESH_OK] = {"À JOUR", "UP TO DATE", "AL DÍA", "AKTUELL", "AGGIORNATO", "ATUALIZADO"},
[S_FRESH_LATE] = {"RETARD", "LATE", "RETRASO", "VERSPÄTET", "IN RITARDO", "ATRASADO"},
[S_FRESH_STALE] = {"DONNÉES ANCIENNES", "OLD DATA", "DATOS ANTIGUOS", "ALTE DATEN",
                             "DATI VECCHI", "DADOS ANTIGOS"},
[S_FRESH_WAITING] = {"EN ATTENTE", "WAITING", "EN ESPERA", "WARTEN", "IN ATTESA", "A AGUARDAR"},

[S_TREND_UP_FAST] = {"Monte vite", "Rising fast", "Sube rápido", "Steigt schnell",
                             "Sale veloce", "Sobe rápido"},
[S_TREND_UP] = {"Monte", "Rising", "Sube", "Steigt", "Sale", "Sobe"},
[S_TREND_UP_SLOW] = {"Monte lentement", "Rising slowly", "Sube despacio", "Steigt langsam",
                             "Sale lentamente", "Sobe devagar"},
[S_TREND_FLAT] = {"Stable", "Steady", "Estable", "Stabil", "Stabile", "Estável"},
[S_TREND_DOWN_SLOW] = {"Baisse lentement", "Falling slowly", "Baja despacio", "Fällt langsam",
                             "Scende lentamente", "Desce devagar"},
[S_TREND_DOWN] = {"Baisse", "Falling", "Baja", "Fällt", "Scende", "Desce"},
[S_TREND_DOWN_FAST] = {"Baisse vite", "Falling fast", "Baja rápido", "Fällt schnell",
                             "Scende veloce", "Desce rápido"},
[S_TREND_UNKNOWN] = {"Tendance incalculable", "Trend unavailable", "Tendencia no disponible",
                             "Trend nicht berechenbar", "Tendenza non disponibile", "Tendência indisponível"},
[S_TREND_OUT_OF_RANGE] = {"Variation hors plage", "Rate out of range", "Variación fuera de rango",
                             "Änderung außerhalb", "Variazione fuori scala", "Variação fora do intervalo"},
[S_TREND_DASH] = {"Tendance : --", "Trend: --", "Tendencia: --", "Trend: --",
                             "Tendenza: --", "Tendência: --"},

[S_UNIT_MGDL] = {"mg/dL", "mg/dL", "mg/dL", "mg/dL", "mg/dL", "mg/dL"},
[S_NO_READING] = {"Aucune mesure reçue", "No reading received", "Sin lecturas recibidas",
                             "Kein Messwert empfangen", "Nessuna misura ricevuta", "Nenhuma leitura recebida"},
[S_WIFI_NOT_CONFIGURED] = {"Wi-Fi non configuré", "Wi-Fi not configured", "Wi-Fi no configurado",
                             "WLAN nicht eingerichtet", "Wi-Fi non configurato", "Wi-Fi não configurado"},
[S_WIFI_DOWN] = {"Wi-Fi coupé", "Wi-Fi down", "Wi-Fi caído", "WLAN unterbrochen",
                             "Wi-Fi assente", "Wi-Fi em baixo"},
[S_DEXCOM_REFUSED] = {"Identifiants Dexcom refusés", "Dexcom login refused", "Credenciales Dexcom rechazadas",
                             "Dexcom-Zugang abgelehnt", "Credenziali Dexcom rifiutate",
                             "Credenciais Dexcom recusadas"},
[S_DEXCOM_LOCKED] = {"Compte Dexcom bloqué", "Dexcom account locked", "Cuenta Dexcom bloqueada",
                             "Dexcom-Konto gesperrt", "Account Dexcom bloccato", "Conta Dexcom bloqueada"},
[S_DEXCOM_CHECK] = {"Connexion Dexcom à vérifier", "Check Dexcom connection", "Revisar conexión Dexcom",
                             "Dexcom-Verbindung prüfen", "Verificare connessione Dexcom",
                             "Verificar ligação Dexcom"},
[S_DEXCOM_UNAVAILABLE] = {"Dexcom indisponible", "Dexcom unavailable", "Dexcom no disponible",
                             "Dexcom nicht erreichbar", "Dexcom non disponibile", "Dexcom indisponível"},
[S_INTERNET_DOWN] = {"Internet coupé", "Internet down", "Internet caído", "Internet unterbrochen",
                             "Internet assente", "Internet em baixo"},
[S_NO_NEW_READING] = {"Aucune nouvelle mesure reçue", "No new reading received", "Sin lecturas nuevas",
                             "Kein neuer Messwert", "Nessuna nuova misura", "Sem leituras novas"},
[S_WIFI_SETUP_NEEDED] = {"Configuration Wi-Fi requise", "Wi-Fi setup required", "Configuración Wi-Fi necesaria",
                             "WLAN-Einrichtung nötig", "Configurazione Wi-Fi necessaria",
                             "Configuração Wi-Fi necessária"},
[S_WIFI_CONNECTING] = {"Connexion Wi-Fi...", "Connecting to Wi-Fi...", "Conectando al Wi-Fi...",
                             "WLAN-Verbindung...", "Connessione Wi-Fi...", "A ligar ao Wi-Fi..."},
[S_DEXCOM_NO_CREDENTIALS] = {"Identifiants Dexcom absents", "Dexcom credentials missing", "Faltan credenciales Dexcom",
                             "Dexcom-Zugangsdaten fehlen", "Credenziali Dexcom assenti",
                             "Credenciais Dexcom em falta"},
[S_DEXCOM_WAITING_AUTH] = {"Dexcom en attente d'autorisation", "Dexcom awaiting authorisation",
                             "Dexcom esperando autorización", "Dexcom wartet auf Freigabe",
                             "Dexcom in attesa di autorizzazione", "Dexcom a aguardar autorização"},
[S_DEXCOM_RETRYING] = {"Dexcom indisponible, nouvel essai", "Dexcom unavailable, retrying",
                             "Dexcom no disponible, reintentando", "Dexcom nicht erreichbar, neuer Versuch",
                             "Dexcom non disponibile, nuovo tentativo", "Dexcom indisponível, a tentar de novo"},
[S_NO_READING_24H] = {"Aucune mesure sur 24 h", "No reading in 24 h", "Sin lecturas en 24 h",
                             "Kein Messwert in 24 h", "Nessuna misura in 24 h", "Sem leituras em 24 h"},
[S_DEXCOM_CONNECTING] = {"Connexion Dexcom...", "Connecting to Dexcom...", "Conectando a Dexcom...",
                             "Dexcom-Verbindung...", "Connessione a Dexcom...", "A ligar à Dexcom..."},

[S_GRAPH_HINT] = {"Zone cible 70–180 mg/dL · appui sur la courbe : détail d'une mesure",
                             "Target range 70–180 mg/dL · tap the curve for a reading",
                             "Rango objetivo 70–180 mg/dL · toque la curva para ver una lectura",
                             "Zielbereich 70–180 mg/dL · Kurve antippen für einen Messwert",
                             "Intervallo 70–180 mg/dL · tocca la curva per una misura",
                             "Intervalo-alvo 70–180 mg/dL · toque na curva para ver uma leitura"},
[S_NO_MEASURE] = {"Aucune mesure", "No reading", "Sin lecturas", "Kein Messwert",
                             "Nessuna misura", "Sem leituras"},

[S_TODAY_TITLE] = {"AUJOURD'HUI", "TODAY", "HOY", "HEUTE", "OGGI", "HOJE"},
[S_SINCE_MIDNIGHT] = {"Depuis minuit", "Since midnight", "Desde medianoche", "Seit Mitternacht",
                             "Da mezzanotte", "Desde a meia-noite"},
[S_DAY_BAND] = {"JOURNÉE", "DAY", "DÍA", "TAG", "GIORNATA", "DIA"},
[S_LEG_TARGET] = {"Cible", "Target", "Objetivo", "Zielbereich", "Obiettivo", "Alvo"},
[S_LEG_LOW] = {"Bas", "Low", "Bajo", "Niedrig", "Basso", "Baixo"},
[S_LEG_HIGH] = {"Haut", "High", "Alto", "Hoch", "Alto", "Alto"},
[S_LEG_NODATA] = {"Sans donnée", "No data", "Sin datos", "Keine Daten", "Senza dati", "Sem dados"},
[S_TIR] = {"TIR", "TIR", "TIR", "TIR", "TIR", "TIR"},
[S_RANGE_70_180] = {"70–180 mg/dL", "70–180 mg/dL", "70–180 mg/dL", "70–180 mg/dL",
                             "70–180 mg/dL", "70–180 mg/dL"},
[S_AVERAGE] = {"MOYENNE", "AVERAGE", "MEDIA", "MITTELWERT", "MEDIA", "MÉDIA"},
[S_MINIMUM] = {"MINIMUM", "MINIMUM", "MÍNIMO", "MINIMUM", "MINIMO", "MÍNIMO"},
[S_MAXIMUM] = {"MAXIMUM", "MAXIMUM", "MÁXIMO", "MAXIMUM", "MASSIMO", "MÁXIMO"},
[S_COVERAGE] = {"COUVERTURE", "COVERAGE", "COBERTURA", "ABDECKUNG", "COPERTURA", "COBERTURA"},
[S_TIME_LOW] = {"TEMPS BAS", "TIME LOW", "TIEMPO BAJO", "ZEIT NIEDRIG", "TEMPO BASSO", "TEMPO BAIXO"},
[S_UNDER_70] = {"< 70 mg/dL", "< 70 mg/dL", "< 70 mg/dL", "< 70 mg/dL", "< 70 mg/dL", "< 70 mg/dL"},
[S_TIME_HIGH] = {"TEMPS HAUT", "TIME HIGH", "TIEMPO ALTO", "ZEIT HOCH", "TEMPO ALTO", "TEMPO ALTO"},
[S_OVER_180] = {"> 180 mg/dL", "> 180 mg/dL", "> 180 mg/dL", "> 180 mg/dL",
                             "> 180 mg/dL", "> 180 mg/dL"},
[S_EPISODES_LOW] = {"ÉPISODES BAS", "LOW EVENTS", "EPISODIOS BAJOS", "UNTERZUCKER",
                             "EPISODI BASSI", "EPISÓDIOS BAIXOS"},
[S_EPISODES_HIGH] = {"ÉPISODES HAUTS", "HIGH EVENTS", "EPISODIOS ALTOS", "ÜBERZUCKER",
                             "EPISODI ALTI", "EPISÓDIOS ALTOS"},
[S_AT_LEAST_15MIN] = {"au moins 15 min", "at least 15 min", "al menos 15 min", "mindestens 15 min",
                             "almeno 15 min", "pelo menos 15 min"},
[S_MEASURES] = {"%d mesures", "%d readings", "%d lecturas", "%d Messwerte",
                             "%d misure", "%d leituras"},

[S_SENSOR_G7] = {"CAPTEUR G7", "G7 SENSOR", "SENSOR G7", "G7-SENSOR", "SENSORE G7", "SENSOR G7"},
[S_SENSOR_SUB] = {"10 jours · rappels 24 h et 2 h avant", "10 days · reminders 24 h and 2 h before",
                             "10 días · avisos 24 h y 2 h antes", "10 Tage · Erinnerung 24 h und 2 h vorher",
                             "10 giorni · avvisi 24 h e 2 h prima", "10 dias · avisos 24 h e 2 h antes"},
[S_NEW_SENSOR] = {"NOUVEAU CAPTEUR", "NEW SENSOR", "NUEVO SENSOR", "NEUER SENSOR",
                             "NUOVO SENSORE", "NOVO SENSOR"},
[S_INFUSION_SITE] = {"SITE DE PERFUSION", "INFUSION SITE", "SITIO DE INFUSIÓN", "KATHETER",
                             "SITO DI INFUSIONE", "LOCAL DE INFUSÃO"},
[S_NEW_CATHETER] = {"NOUVEAU CATHÉTER", "NEW CATHETER", "NUEVO CATÉTER", "NEUER KATHETER",
                             "NUOVO CATETERE", "NOVO CATETER"},
[S_STOCK_SENSORS] = {"STOCK CAPTEURS G7", "G7 SENSORS IN STOCK", "SENSORES G7 EN STOCK",
                             "VORRAT G7-SENSOREN", "SCORTA SENSORI G7", "STOCK DE SENSORES G7"},
[S_STOCK_CATHETERS] = {"STOCK CATHÉTERS", "CATHETERS IN STOCK", "CATÉTERES EN STOCK",
                             "VORRAT KATHETER", "SCORTA CATETERI", "STOCK DE CATETERES"},
[S_STOCK_LOW] = {"STOCK FAIBLE", "LOW STOCK", "STOCK BAJO", "VORRAT NIEDRIG",
                             "SCORTA BASSA", "STOCK BAIXO"},
[S_NOT_SET] = {"NON RENSEIGNÉ", "NOT SET", "SIN INDICAR", "NICHT ANGEGEBEN",
                             "NON INDICATO", "POR INDICAR"},
[S_SET_IT] = {"RENSEIGNER", "SET", "INDICAR", "ANGEBEN", "INDICA", "INDICAR"},
[S_NOT_RECORDED] = {"Non enregistré", "Not recorded", "Sin registrar", "Nicht erfasst",
                             "Non registrato", "Não registado"},
[S_BUTTON_BELOW] = {"Bouton ci-dessous", "Button below", "Botón de abajo", "Schaltfläche unten",
                             "Pulsante qui sotto", "Botão abaixo"},
[S_NO_SENSOR_RECORDED] = {"Aucun capteur enregistré", "No sensor recorded", "Ningún sensor registrado",
                             "Kein Sensor erfasst", "Nessun sensore registrato", "Nenhum sensor registado"},
[S_NO_CATHETER_RECORDED] = {"Aucun cathéter enregistré", "No catheter recorded", "Ningún catéter registrado",
                             "Kein Katheter erfasst", "Nessun catetere registrato", "Nenhum cateter registado"},
[S_TO_CHANGE] = {"À CHANGER", "REPLACE NOW", "CAMBIAR YA", "JETZT WECHSELN",
                             "DA SOSTITUIRE", "SUBSTITUIR JÁ"},
[S_PLACED] = {"Posé", "Inserted", "Colocado", "Gesetzt", "Inserito", "Colocado"},
[S_CHANGED] = {"Changé", "Changed", "Cambiado", "Gewechselt", "Cambiato", "Mudado"},
[S_DAY_N_OF] = {"Jour %d / %d", "Day %d / %d", "Día %d / %d", "Tag %d / %d",
                             "Giorno %d / %d", "Dia %d / %d"},
[S_TIME_LEFT] = {"Temps restant", "Time left", "Tiempo restante", "Restzeit",
                             "Tempo rimasto", "Tempo restante"},
[S_DUE] = {"Échéance", "Due", "Vencimiento", "Fällig", "Scadenza", "Prazo"},
[S_OVERDUE_SINCE] = {"Dépassée depuis", "Overdue by", "Vencido hace", "Überfällig seit",
                             "Scaduta da", "Vencido há"},
[S_NEW_SENSOR_NOW] = {"Nouveau capteur maintenant ?", "New sensor now?", "¿Nuevo sensor ahora?",
                             "Neuer Sensor jetzt?", "Nuovo sensore adesso?", "Novo sensor agora?"},
[S_NEW_CATHETER_NOW] = {"Nouveau cathéter maintenant ?", "New catheter now?", "¿Nuevo catéter ahora?",
                             "Neuer Katheter jetzt?", "Nuovo catetere adesso?", "Novo cateter agora?"},
[S_RESTART_DAY1_SENSOR] = {"Le suivi repart à Jour 1 / 10.", "Tracking restarts at day 1 / 10.",
                             "El seguimiento vuelve al día 1 / 10.", "Die Zählung beginnt bei Tag 1 / 10.",
                             "Il conteggio riparte dal giorno 1 / 10.", "A contagem reinicia no dia 1 / 10."},
[S_RESTART_DAY1_CATHETER] = {"Le suivi repart à Jour 1 (rappel 2 h avant l'échéance).",
                             "Tracking restarts at day 1 (reminder 2 h before it is due).",
                             "El seguimiento vuelve al día 1 (aviso 2 h antes del vencimiento).",
                             "Die Zählung beginnt bei Tag 1 (Erinnerung 2 h vor Fälligkeit).",
                             "Il conteggio riparte dal giorno 1 (avviso 2 h prima della scadenza).",
                             "A contagem reinicia no dia 1 (aviso 2 h antes do prazo)."},
[S_CANCEL] = {"ANNULER", "CANCEL", "CANCELAR", "ABBRECHEN", "ANNULLA", "CANCELAR"},
[S_YES_NOW] = {"OUI, MAINTENANT", "YES, NOW", "SÍ, AHORA", "JA, JETZT", "SÌ, ADESSO", "SIM, AGORA"},
[S_VALIDATE] = {"VALIDER", "CONFIRM", "ACEPTAR", "BESTÄTIGEN", "CONFERMA", "CONFIRMAR"},
[S_TIME_NOT_SYNCED_NO_RECORD] = {"Heure non synchronisée : enregistrement impossible",
                             "Clock not synchronised: cannot record",
                             "Hora no sincronizada: no se puede registrar",
                             "Uhrzeit nicht synchronisiert: Speichern nicht möglich",
                             "Ora non sincronizzata: registrazione impossibile",
                             "Hora não sincronizada: não é possível registar"},

[S_START_TIME_LABEL] = {"Heure de départ", "Start time", "Hora de inicio", "Startzeit",
                        "Ora di inizio", "Hora de inicio"},
[S_CATHETER_SUB] = {"Cathéter · %d h · rappel 2 h avant", "Catheter · %d h · reminder 2 h before",
                    "Catéter · %d h · aviso 2 h antes", "Katheter · %d h · Erinnerung 2 h vorher",
                    "Catetere · %d h · avviso 2 h prima", "Cateter · %d h · aviso 2 h antes"},
[S_VOLUME_TITLE] = {"VOLUME GÉNÉRAL", "MASTER VOLUME", "VOLUMEN GENERAL", "LAUTSTÄRKE",
                             "VOLUME GENERALE", "VOLUME GERAL"},
[S_VOLUME_PCT] = {"Volume : %d %%", "Volume: %d %%", "Volumen: %d %%", "Lautstärke: %d %%",
                             "Volume: %d %%", "Volume: %d %%"},
[S_VOLUME_MUTED] = {"Volume : 0 % (muet)", "Volume: 0 % (muted)", "Volumen: 0 % (silencio)",
                             "Lautstärke: 0 % (stumm)", "Volume: 0 % (muto)", "Volume: 0 % (sem som)"},
[S_MUTE] = {"MUTE", "MUTE", "SILENCIO", "STUMM", "MUTO", "SEM SOM"},
[S_LOW] = {"FAIBLE", "LOW", "BAJO", "LEISE", "BASSO", "BAIXO"},
[S_MEDIUM] = {"MOYEN", "MEDIUM", "MEDIO", "MITTEL", "MEDIO", "MÉDIO"},
[S_HIGH] = {"FORT", "HIGH", "ALTO", "LAUT", "ALTO", "ALTO"},
[S_MAX] = {"MAX", "MAX", "MÁX", "MAX", "MAX", "MÁX"},
[S_TEST_SOUND] = {"TEST SON", "TEST SOUND", "PROBAR SONIDO", "TON TESTEN",
                             "PROVA SUONO", "TESTAR SOM"},
[S_TEST_VOICE] = {"TEST VOIX", "TEST VOICE", "PROBAR VOZ", "STIMME TESTEN",
                             "PROVA VOCE", "TESTAR VOZ"},
[S_VOICE_ANNOUNCE] = {"Annonces vocales", "Spoken announcements", "Avisos por voz", "Sprachansagen",
                             "Annunci vocali", "Anúncios por voz"},
[S_ALERTS_TITLE] = {"ALERTES", "ALERTS", "ALERTAS", "ALARME", "AVVISI", "ALERTAS"},
[S_ALERTS_SOUND] = {"Alertes sonores", "Audible alerts", "Alertas sonoras", "Akustische Alarme",
                             "Avvisi sonori", "Alertas sonoros"},
[S_RAMP] = {"Volume croissant si alerte très basse non acquittée",
                             "Rising volume while an urgent low alert is unacknowledged",
                             "Volumen creciente si no se confirma una alerta muy baja",
                             "Lauter werden, solange ein Unterzucker-Alarm nicht bestätigt ist",
                             "Volume crescente se un avviso molto basso non è confermato",
                             "Volume crescente se um alerta muito baixo não for confirmado"},
[S_VERYLOW_IN_MUTE] = {"Alerte très basse audible même en MUTE",
                             "Urgent low alert audible even when muted",
                             "Alerta muy baja audible incluso en silencio",
                             "Unterzucker-Alarm auch bei Stumm hörbar",
                             "Avviso molto basso udibile anche in muto",
                             "Alerta muito baixo audível mesmo sem som"},
[S_THRESHOLDS] = {"Seuils (mg/dL) :", "Thresholds (mg/dL):", "Umbrales (mg/dL):",
                             "Grenzwerte (mg/dL):", "Soglie (mg/dL):", "Limites (mg/dL):"},
[S_THR_VERY_LOW] = {"TRÈS BASSE <", "URGENT LOW <", "MUY BAJA <", "SEHR NIEDRIG <",
                             "MOLTO BASSA <", "MUITO BAIXA <"},
[S_THR_LOW] = {"BASSE <", "LOW <", "BAJA <", "NIEDRIG <", "BASSA <", "BAIXA <"},
[S_THR_HIGH] = {"ÉLEVÉE >", "HIGH >", "ALTA >", "HOCH >", "ALTA >", "ALTA >"},
[S_THR_VERY_HIGH] = {"TRÈS ÉLEVÉE >", "URGENT HIGH >", "MUY ALTA >", "SEHR HOCH >",
                             "MOLTO ALTA >", "MUITO ALTA >"},
[S_SCREEN_TITLE] = {"ÉCRAN", "SCREEN", "PANTALLA", "BILDSCHIRM", "SCHERMO", "ECRÃ"},
[S_DAY_BRIGHTNESS] = {"Luminosité jour", "Daytime brightness", "Brillo de día", "Helligkeit Tag",
                             "Luminosità giorno", "Brilho de dia"},
[S_NIGHT_TITLE] = {"MODE NUIT", "NIGHT MODE", "MODO NOCHE", "NACHTMODUS",
                             "MODO NOTTE", "MODO NOTURNO"},
[S_NIGHT_ENABLE] = {"Mode nuit (selon l'horaire)", "Night mode (on schedule)", "Modo noche (según horario)",
                             "Nachtmodus (nach Zeitplan)", "Modo notte (secondo orario)",
                             "Modo noturno (conforme horário)"},
[S_NIGHT_START] = {"Début", "Start", "Inicio", "Beginn", "Inizio", "Início"},
[S_NIGHT_END] = {"Fin", "End", "Fin", "Ende", "Fine", "Fim"},
[S_NIGHT_BRIGHTNESS] = {"Luminosité nuit", "Night brightness", "Brillo de noche", "Helligkeit Nacht",
                             "Luminosità notte", "Brilho noturno"},
[S_NIGHT_TEST] = {"TEST MODE NUIT", "PREVIEW NIGHT MODE", "PROBAR MODO NOCHE",
                             "NACHTMODUS TESTEN", "PROVA MODO NOTTE", "TESTAR MODO NOTURNO"},
[S_NIGHT_HELP] = {"La nuit : écran simplifié (heure, glycémie, tendance). Un toucher affiche "
                             "l'interface normale 30 s ; les alertes restent prioritaires.",
                             "At night: simplified screen (time, glucose, trend). A touch shows the normal "
                             "interface for 30 s; alerts still take priority.",
                             "De noche: pantalla simplificada (hora, glucosa, tendencia). Un toque muestra "
                             "la interfaz normal 30 s; las alertas siguen teniendo prioridad.",
                             "Nachts: vereinfachte Anzeige (Uhrzeit, Glukose, Trend). Eine Berührung zeigt "
                             "30 s die normale Oberfläche; Alarme haben weiter Vorrang.",
                             "Di notte: schermo semplificato (ora, glicemia, tendenza). Un tocco mostra "
                             "l'interfaccia normale per 30 s; gli avvisi restano prioritari.",
                             "À noite: ecrã simplificado (hora, glicemia, tendência). Um toque mostra "
                             "a interface normal 30 s; os alertas mantêm prioridade."},
[S_CONSUM_TITLE] = {"CONSOMMABLES", "SUPPLIES", "FUNGIBLES", "VERBRAUCHSMATERIAL",
                             "MATERIALI", "CONSUMÍVEIS"},
[S_CATHETER_REMINDER] = {"Rappel du cathéter (site de perfusion)", "Catheter reminder (infusion site)",
                             "Aviso del catéter (sitio de infusión)", "Katheter-Erinnerung (Infusionsstelle)",
                             "Avviso catetere (sito di infusione)", "Aviso do cateter (local de infusão)"},
[S_CATHETER_HELP] = {"Rappel unique 2 h avant l'échéance "
                             "(rappel choisi par l'utilisateur, pas une recommandation).",
                             "Single reminder 2 h before it is due "
                             "(a reminder you chose, not a medical recommendation).",
                             "Aviso único 2 h antes del vencimiento "
                             "(aviso elegido por usted, no una recomendación médica).",
                             "Einmalige Erinnerung 2 h vor Fälligkeit "
                             "(selbst gewählt, keine medizinische Empfehlung).",
                             "Avviso unico 2 h prima della scadenza "
                             "(scelto dall'utente, non una raccomandazione medica).",
                             "Aviso único 2 h antes do prazo "
                             "(escolhido por si, não é uma recomendação médica)."},
[S_VOICECMD_TITLE] = {"COMMANDES VOCALES", "VOICE COMMANDS", "COMANDOS DE VOZ", "SPRACHBEFEHLE",
                             "COMANDI VOCALI", "COMANDOS DE VOZ"},
[S_VOICECMD_ENABLE] = {"Commandes vocales (micros, reconnaissance locale)",
                             "Voice commands (microphones, offline recognition)",
                             "Comandos de voz (micrófonos, reconocimiento local)",
                             "Sprachbefehle (Mikrofone, lokale Erkennung)",
                             "Comandi vocali (microfoni, riconoscimento locale)",
                             "Comandos de voz (microfones, reconhecimento local)"},
[S_WAKEWORD] = {"Mot d'éveil « Bonjour ESP » avant la commande",
                             "Wake word \"Bonjour ESP\" before the command",
                             "Palabra de activación «Bonjour ESP» antes del comando",
                             "Aktivierungswort „Bonjour ESP“ vor dem Befehl",
                             "Parola di attivazione «Bonjour ESP» prima del comando",
                             "Palavra de ativação «Bonjour ESP» antes do comando"},
[S_VOICECMD_HELP] = {"Les commandes se disent en français : « Glycémie », « Glucose », "
                             "« Enregistrer nouveau capteur » puis « Confirmer capteur », « Nouveau cathéter » "
                             "(à répéter pour confirmer). Les réponses sont toujours en français.",
                             "Commands are spoken in French: \"Glycemie\", \"Glucose\", "
                             "\"Enregistrer nouveau capteur\" then \"Confirmer capteur\", \"Nouveau catheter\" "
                             "(repeat to confirm). Spoken replies are always in French.",
                             "Los comandos se dicen en francés: «Glycemie», «Glucose», "
                             "«Enregistrer nouveau capteur» y luego «Confirmer capteur», «Nouveau catheter» "
                             "(repetir para confirmar). Las respuestas son siempre en francés.",
                             "Die Befehle werden auf Französisch gesprochen: „Glycemie“, „Glucose“, "
                             "„Enregistrer nouveau capteur“, dann „Confirmer capteur“, "
                             "„Nouveau catheter“ (zum Bestätigen wiederholen). Antworten immer auf Französisch.",
                             "I comandi si pronunciano in francese: «Glycemie», «Glucose», "
                             "«Enregistrer nouveau capteur» poi «Confirmer capteur», «Nouveau catheter» "
                             "(ripetere per confermare). Le risposte sono sempre in francese.",
                             "Os comandos dizem-se em francês: «Glycemie», «Glucose», "
                             "«Enregistrer nouveau capteur» e depois «Confirmer capteur», «Nouveau catheter» "
                             "(repetir para confirmar). As respostas são sempre em francês."},
[S_INFO_TITLE] = {"INFORMATIONS", "INFORMATION", "INFORMACIÓN", "INFORMATIONEN",
                             "INFORMAZIONI", "INFORMAÇÕES"},
[S_LANG_TITLE] = {"LANGUE", "LANGUAGE", "IDIOMA", "SPRACHE", "LINGUA", "IDIOMA"},
[S_LANG_HELP] = {"Langue de l'écran et des annonces parlées. La banque vocale de cette langue doit "
                             "être sur la microSD ; sinon la voix française prend le relais.",
                             "Language of the screen and of the spoken announcements. That language's voice bank "
                             "must be on the microSD; otherwise the French voice takes over.",
                             "Idioma de la pantalla y de los avisos hablados. El banco de voz de ese idioma debe "
                             "estar en la microSD; si no, se usa la voz francesa.",
                             "Sprache der Anzeige und der Sprachansagen. Die Sprachbank dieser Sprache muss auf "
                             "der microSD liegen, sonst übernimmt die französische Stimme.",
                             "Lingua dello schermo e degli annunci vocali. La banca vocale di questa lingua deve "
                             "essere sulla microSD; altrimenti subentra la voce francese.",
                             "Idioma do ecrã e dos anúncios falados. O banco de voz deste idioma deve estar no "
                             "cartão microSD; caso contrário, a voz francesa assume."},
[S_INFO_VERSION] = {"Version %s, ESP-IDF %s\n", "Version %s, ESP-IDF %s\n", "Versión %s, ESP-IDF %s\n",
                             "Version %s, ESP-IDF %s\n", "Versione %s, ESP-IDF %s\n", "Versão %s, ESP-IDF %s\n"},
[S_INFO_WIFI] = {"Wi-Fi : %s, IP %s, signal %d dBm, firmware ESP32-C6 : %s\n",
                             "Wi-Fi: %s, IP %s, signal %d dBm, ESP32-C6 firmware: %s\n",
                             "Wi-Fi: %s, IP %s, señal %d dBm, firmware ESP32-C6: %s\n",
                             "WLAN: %s, IP %s, Signal %d dBm, ESP32-C6-Firmware: %s\n",
                             "Wi-Fi: %s, IP %s, segnale %d dBm, firmware ESP32-C6: %s\n",
                             "Wi-Fi: %s, IP %s, sinal %d dBm, firmware ESP32-C6: %s\n"},
[S_INFO_DEXCOM] = {"Dexcom : %s\n", "Dexcom: %s\n", "Dexcom: %s\n", "Dexcom: %s\n",
                             "Dexcom: %s\n", "Dexcom: %s\n"},
[S_INFO_SD] = {"microSD : %s\n", "microSD: %s\n", "microSD: %s\n", "microSD: %s\n",
                             "microSD: %s\n", "microSD: %s\n"},
[S_INFO_AUDIO] = {"Audio : %s, voix : %s", "Audio: %s, voice: %s", "Audio: %s, voz: %s",
                             "Audio: %s, Stimme: %s", "Audio: %s, voce: %s", "Áudio: %s, voz: %s"},
[S_CONNECTED] = {"connecté", "connected", "conectado", "verbunden", "connesso", "ligado"},
[S_DISCONNECTED] = {"déconnecté", "disconnected", "desconectado", "getrennt",
                             "disconnesso", "desligado"},
[S_CONNECTED_EUROPE] = {"connecté (Europe)", "connected (Europe)", "conectado (Europa)",
                             "verbunden (Europa)", "connesso (Europa)", "ligado (Europa)"},
[S_NOT_CONNECTED] = {"non connecté", "not connected", "no conectado", "nicht verbunden",
                             "non connesso", "não ligado"},
[S_READY] = {"prêt", "ready", "listo", "bereit", "pronto", "pronto"},

[S_STATE] = {"État", "Status", "Estado", "Status", "Stato", "Estado"},
[S_UNAVAILABLE] = {"indisponible", "unavailable", "no disponible", "nicht verfügbar", "non disponibile", "indisponível"},
[S_WAKEWORD_NOTE] = {"Mot d'éveil « Bonjour ESP » exigé avant chaque commande.", "Wake word \"Bonjour ESP\" required before every command.", "Palabra de activación «Bonjour ESP» obligatoria antes de cada comando.", "Aktivierungswort „Bonjour ESP“ vor jedem Befehl erforderlich.", "Parola di attivazione «Bonjour ESP» richiesta prima di ogni comando.", "Palavra de ativação «Bonjour ESP» obrigatória antes de cada comando."},
[S_SD_OK_FMT] = {"%s %u Mo, %u Mo libres, %u mesures écrites", "%s %u MB, %u MB free, %u readings written", "%s %u MB, %u MB libres, %u lecturas escritas", "%s %u MB, %u MB frei, %u Messwerte geschrieben", "%s %u MB, %u MB liberi, %u misure scritte", "%s %u MB, %u MB livres, %u leituras escritas"},
[S_SD_ABSENT] = {"indisponible (absente ou retirée), nouvel essai automatique", "unavailable (missing or removed), will retry automatically", "no disponible (ausente o retirada), reintento automático", "nicht verfügbar (fehlt oder entfernt), automatischer neuer Versuch", "non disponibile (assente o rimossa), nuovo tentativo automatico", "indisponível (ausente ou removida), nova tentativa automática"},
[S_SD_UNSUPPORTED_FMT] = {"%s %u Mo, système de fichiers non reconnu, aucun formatage", "%s %u MB, unrecognised file system, no formatting", "%s %u MB, sistema de archivos no reconocido, sin formatear", "%s %u MB, Dateisystem nicht erkannt, keine Formatierung", "%s %u MB, file system non riconosciuto, nessuna formattazione", "%s %u MB, sistema de ficheiros não reconhecido, sem formatação"},
[S_SD_UNSUPPORTED] = {"système de fichiers non reconnu (aucun formatage automatique)", "unrecognised file system (no automatic formatting)", "sistema de archivos no reconocido (sin formateo automático)", "Dateisystem nicht erkannt (keine automatische Formatierung)", "file system non riconosciuto (nessuna formattazione automatica)", "sistema de ficheiros não reconhecido (sem formatação automática)"},
[S_SD_ERROR] = {"erreur d'accès, nouvel essai automatique", "access error, will retry automatically", "error de acceso, reintento automático", "Zugriffsfehler, automatischer neuer Versuch", "errore di accesso, nuovo tentativo automatico", "erro de acesso, nova tentativa automática"},
[S_SD_INIT] = {"initialisation...", "starting up...", "inicializando...", "Initialisierung...", "inizializzazione...", "a inicializar..."},
[S_VOICE_FILES_FMT] = {"%d fichiers WAV sur la microSD", "%d WAV files on the microSD", "%d archivos WAV en la microSD", "%d WAV-Dateien auf der microSD", "%d file WAV sulla microSD", "%d ficheiros WAV no microSD"},
[S_VOICE_ABSENT] = {"absente (bip d'alerte seul)", "missing (alert beep only)", "ausente (solo pitido de alerta)", "fehlt (nur Alarmton)", "assente (solo segnale acustico)", "ausente (apenas aviso sonoro)"},
[S_ALERT_VERY_LOW] = {"GLYCÉMIE TRÈS BASSE", "URGENT LOW GLUCOSE", "GLUCOSA MUY BAJA",
                             "GLUKOSE SEHR NIEDRIG", "GLICEMIA MOLTO BASSA", "GLICEMIA MUITO BAIXA"},
[S_ALERT_LOW] = {"GLYCÉMIE BASSE", "LOW GLUCOSE", "GLUCOSA BAJA", "GLUKOSE NIEDRIG",
                             "GLICEMIA BASSA", "GLICEMIA BAIXA"},
[S_ALERT_NO_DATA] = {"AUCUNE DONNÉE", "NO DATA", "SIN DATOS", "KEINE DATEN",
                             "NESSUN DATO", "SEM DADOS"},
[S_ALERT_VERY_HIGH] = {"GLYCÉMIE TRÈS ÉLEVÉE", "URGENT HIGH GLUCOSE", "GLUCOSA MUY ALTA",
                             "GLUKOSE SEHR HOCH", "GLICEMIA MOLTO ALTA", "GLICEMIA MUITO ALTA"},
[S_ALERT_HIGH] = {"GLYCÉMIE ÉLEVÉE", "HIGH GLUCOSE", "GLUCOSA ALTA", "GLUKOSE HOCH",
                             "GLICEMIA ALTA", "GLICEMIA ALTA"},
[S_ALERT_SENSOR] = {"CAPTEUR G7", "G7 SENSOR", "SENSOR G7", "G7-SENSOR", "SENSORE G7", "SENSOR G7"},
[S_ALERT_CATHETER] = {"CATHÉTER", "CATHETER", "CATÉTER", "KATHETER", "CATETERE", "CATETER"},
[S_ALERT_TEST] = {"ALERTE TEST", "TEST ALERT", "ALERTA DE PRUEBA", "TESTALARM",
                             "AVVISO DI PROVA", "ALERTA DE TESTE"},
[S_ALERT_TEST_SUFFIX] = {" (TEST)", " (TEST)", " (PRUEBA)", " (TEST)", " (PROVA)", " (TESTE)"},
[S_ALERT_TEST_NOTE] = {"Alerte de test (glycémie Dexcom non modifiée)",
                             "Test alert (Dexcom reading unchanged)",
                             "Alerta de prueba (lectura Dexcom sin cambios)",
                             "Testalarm (Dexcom-Wert unverändert)",
                             "Avviso di prova (misura Dexcom invariata)",
                             "Alerta de teste (leitura Dexcom inalterada)"},
[S_ALERT_OK] = {"OK", "OK", "OK", "OK", "OK", "OK"},
[S_ALERT_SNOOZE] = {"RAPPEL DANS 30 MIN", "REMIND IN 30 MIN", "AVISAR EN 30 MIN",
                             "IN 30 MIN ERINNERN", "RICORDA TRA 30 MIN", "LEMBRAR EM 30 MIN"},
[S_REMIND_SENSOR_24H] = {"Le capteur doit être changé demain", "The sensor must be replaced tomorrow",
                             "El sensor debe cambiarse mañana", "Der Sensor muss morgen gewechselt werden",
                             "Il sensore va sostituito domani", "O sensor deve ser substituído amanhã"},
[S_REMIND_SENSOR_2H] = {"Le capteur doit être changé dans 2 h", "The sensor must be replaced in 2 h",
                             "El sensor debe cambiarse en 2 h", "Der Sensor muss in 2 h gewechselt werden",
                             "Il sensore va sostituito tra 2 h", "O sensor deve ser substituído em 2 h"},
[S_REMIND_CATHETER_2H] = {"Le cathéter doit être changé dans 2 h", "The catheter must be changed in 2 h",
                             "El catéter debe cambiarse en 2 h", "Der Katheter muss in 2 h gewechselt werden",
                             "Il catetere va sostituito tra 2 h", "O cateter deve ser mudado em 2 h"},

[S_ALERT_NO_DATA_MSG] = {"Aucune nouvelle mesure depuis %d min", "No new reading for %d min",
                         "Sin lecturas nuevas desde hace %d min", "Seit %d Min kein neuer Messwert",
                         "Nessuna nuova misura da %d min", "Sem leituras novas há %d min"},
[S_NIGHT_PREVIEW] = {"APERÇU MODE NUIT\ntoucher l'écran pour quitter",
                             "NIGHT MODE PREVIEW\ntouch the screen to exit",
                             "VISTA PREVIA MODO NOCHE\ntoque la pantalla para salir",
                             "VORSCHAU NACHTMODUS\nBildschirm berühren zum Beenden",
                             "ANTEPRIMA MODO NOTTE\ntocca lo schermo per uscire",
                             "PRÉ-VISUALIZAÇÃO MODO NOTURNO\ntoque no ecrã para sair"},
[S_LAST_READING] = {"Dernière mesure", "Last reading", "Última lectura", "Letzter Messwert",
                             "Ultima misura", "Última leitura"},

[S_DUR_DH] = {"%d j %d h", "%dd %dh", "%d d %d h", "%d T %d Std",
                             "%d g %d h", "%d d %d h"},
[S_DUR_HM] = {"%d h %02d", "%dh %02d", "%d h %02d", "%d Std %02d",
                             "%d h %02d", "%d h %02d"},
[S_DUR_H] = {"%d h", "%dh", "%d h", "%d Std", "%d h", "%d h"},
[S_DUR_MIN] = {"%d min", "%d min", "%d min", "%d Min", "%d min", "%d min"},
[S_AGE_UNKNOWN] = {"Âge inconnu", "Age unknown", "Antigüedad desconocida", "Alter unbekannt",
                             "Età sconosciuta", "Idade desconhecida"},
[S_AGE_LESS_1MIN] = {"Il y a moins d'1 min", "Less than 1 min ago", "Hace menos de 1 min",
                             "Vor weniger als 1 Min", "Meno di 1 min fa", "Há menos de 1 min"},
[S_AGE_MIN] = {"Il y a %d min", "%d min ago", "Hace %d min", "Vor %d Min",
                             "%d min fa", "Há %d min"},
[S_AGE_HM] = {"Il y a %d h %02d", "%dh %02d ago", "Hace %d h %02d", "Vor %d Std %02d",
                             "%d h %02d fa", "Há %d h %02d"},
[S_DATE_DAY_HM] = {"%02d/%02d à %02d:%02d", "%02d/%02d at %02d:%02d", "%02d/%02d a las %02d:%02d",
                             "%02d.%02d um %02d:%02d", "%02d/%02d alle %02d:%02d", "%02d/%02d às %02d:%02d"},

/* --- Assistant de configuration ------------------------------------------------------------- */
[S_SETUP_WELCOME] = {"Bienvenue", "Welcome", "Bienvenido", "Willkommen", "Benvenuto", "Bem-vindo"},
[S_SETUP_INTRO] = {"Cet écran affiche la glycémie d'un capteur Dexcom G7. Trois étapes suffisent : la langue, le Wi-Fi, le compte Dexcom.",
                             "This display shows glucose from a Dexcom G7 sensor. Three steps: language, Wi-Fi, Dexcom account.",
                             "Esta pantalla muestra la glucosa de un sensor Dexcom G7. Tres pasos: idioma, Wi-Fi y cuenta Dexcom.",
                             "Dieser Bildschirm zeigt den Glukosewert eines Dexcom-G7-Sensors. Drei Schritte: Sprache, WLAN, Dexcom-Konto.",
                             "Questo schermo mostra la glicemia di un sensore Dexcom G7. Tre passaggi: lingua, Wi-Fi, account Dexcom.",
                             "Este ecrã mostra a glicemia de um sensor Dexcom G7. Três passos: idioma, Wi-Fi e conta Dexcom."},
[S_SETUP_LANG] = {"Choisissez votre langue", "Choose your language", "Elija su idioma",
                             "Wählen Sie Ihre Sprache", "Scegli la lingua", "Escolha o seu idioma"},
[S_SETUP_START] = {"Commencer", "Start", "Empezar", "Starten", "Iniziare", "Começar"},
[S_SETUP_DEMO_TRY] = {"Essayer sans compte", "Try without an account", "Probar sin cuenta",
                             "Ohne Konto testen", "Prova senza account", "Experimentar sem conta"},
[S_SETUP_WIFI_TITLE] = {"Votre réseau Wi-Fi", "Your Wi-Fi network", "Su red Wi-Fi",
                             "Ihr WLAN-Netz", "La tua rete Wi-Fi", "A sua rede Wi-Fi"},
[S_SETUP_WIFI_HELP] = {"Choisissez votre box dans la liste. Cet appareil ne fonctionne qu'en 2,4 GHz.",
                             "Pick your router in the list. This device only works on 2.4 GHz.",
                             "Elija su router en la lista. Este aparato solo funciona en 2,4 GHz.",
                             "Wählen Sie Ihren Router aus der Liste. Das Gerät arbeitet nur mit 2,4 GHz.",
                             "Scegli il router nell'elenco. Questo apparecchio funziona solo a 2,4 GHz.",
                             "Escolha o seu router na lista. Este aparelho só funciona em 2,4 GHz."},
[S_SETUP_SCANNING] = {"Recherche des réseaux…", "Scanning for networks…", "Buscando redes…",
                             "Netze werden gesucht…", "Ricerca delle reti…", "A procurar redes…"},
[S_SETUP_NO_NETWORK] = {"Aucun réseau trouvé", "No network found", "No se encontró ninguna red",
                             "Kein Netz gefunden", "Nessuna rete trovata", "Nenhuma rede encontrada"},
[S_SETUP_RESCAN] = {"Rechercher à nouveau", "Scan again", "Buscar de nuevo",
                             "Erneut suchen", "Cerca di nuovo", "Procurar de novo"},
[S_SETUP_HIDDEN] = {"Réseau masqué…", "Hidden network…", "Red oculta…",
                             "Verstecktes Netz…", "Rete nascosta…", "Rede oculta…"},
[S_SETUP_NETWORK_NAME] = {"Nom du réseau", "Network name", "Nombre de la red",
                             "Netzname", "Nome della rete", "Nome da rede"},
[S_SETUP_WIFI_PASS] = {"Mot de passe du Wi-Fi", "Wi-Fi password", "Contraseña del Wi-Fi",
                             "WLAN-Passwort", "Password del Wi-Fi", "Palavra-passe do Wi-Fi"},
[S_SETUP_SHOW] = {"Afficher", "Show", "Mostrar", "Anzeigen", "Mostra", "Mostrar"},
[S_SETUP_DEXCOM_TITLE] = {"Compte Dexcom Share", "Dexcom Share account", "Cuenta Dexcom Share",
                             "Dexcom-Share-Konto", "Account Dexcom Share", "Conta Dexcom Share"},
[S_SETUP_DEXCOM_HELP] = {"Utilisez le compte qui PARTAGE les mesures (celui de l'application Dexcom), pas celui d'un suiveur.",
                             "Use the account that SHARES the readings (the Dexcom app account), not a follower's.",
                             "Use la cuenta que COMPARTE las mediciones (la de la aplicación Dexcom), no la de un seguidor.",
                             "Verwenden Sie das Konto, das die Werte TEILT (das der Dexcom-App), nicht das eines Followers.",
                             "Usa l'account che CONDIVIDE le misure (quello dell'app Dexcom), non quello di un follower.",
                             "Use a conta que PARTILHA as medições (a da aplicação Dexcom), não a de um seguidor."},
[S_SETUP_REGION] = {"Région", "Region", "Región", "Region", "Regione", "Região"},
[S_SETUP_USER] = {"Identifiant Dexcom", "Dexcom username", "Usuario Dexcom",
                             "Dexcom-Benutzername", "Nome utente Dexcom", "Utilizador Dexcom"},
[S_SETUP_PASSWORD] = {"Mot de passe Dexcom", "Dexcom password", "Contraseña Dexcom",
                             "Dexcom-Passwort", "Password Dexcom", "Palavra-passe Dexcom"},
[S_SETUP_CHECK_TITLE] = {"Vérification", "Checking", "Comprobación", "Prüfung", "Verifica", "Verificação"},
[S_SETUP_CHECK_WIFI] = {"Connexion au Wi-Fi…", "Connecting to Wi-Fi…", "Conectando al Wi-Fi…",
                             "Verbinde mit WLAN…", "Connessione al Wi-Fi…", "A ligar ao Wi-Fi…"},
[S_SETUP_CHECK_NET] = {"Accès à Internet…", "Internet access…", "Acceso a Internet…",
                             "Internetzugang…", "Accesso a Internet…", "Acesso à Internet…"},
[S_SETUP_CHECK_DEX] = {"Compte Dexcom…", "Dexcom account…", "Cuenta Dexcom…",
                             "Dexcom-Konto…", "Account Dexcom…", "Conta Dexcom…"},
[S_SETUP_OK] = {"Tout fonctionne.", "Everything works.", "Todo funciona.",
                             "Alles funktioniert.", "Tutto funziona.", "Tudo funciona."},
[S_SETUP_RESTART] = {"L'appareil redémarre…", "The device is restarting…", "El aparato se reinicia…",
                             "Das Gerät startet neu…", "L'apparecchio si riavvia…", "O aparelho reinicia…"},
[S_SETUP_FAIL_WIFI] = {"Connexion impossible. Vérifiez le mot de passe et que la box émet bien en 2,4 GHz.",
                             "Connection failed. Check the password and that the router broadcasts 2.4 GHz.",
                             "Conexión imposible. Compruebe la contraseña y que el router emita en 2,4 GHz.",
                             "Verbindung fehlgeschlagen. Prüfen Sie das Passwort und ob der Router 2,4 GHz sendet.",
                             "Connessione impossibile. Controlla la password e che il router trasmetta a 2,4 GHz.",
                             "Ligação impossível. Verifique a palavra-passe e se o router emite em 2,4 GHz."},
[S_SETUP_FAIL_NET] = {"Wi-Fi connecté, mais pas d'accès à Internet.", "Wi-Fi connected, but no Internet access.",
                             "Wi-Fi conectado, pero sin acceso a Internet.", "WLAN verbunden, aber kein Internetzugang.",
                             "Wi-Fi connesso, ma senza accesso a Internet.", "Wi-Fi ligado, mas sem acesso à Internet."},
[S_SETUP_FAIL_DEX_AUTH] = {"Identifiant ou mot de passe Dexcom refusé.", "Dexcom username or password refused.",
                             "Usuario o contraseña Dexcom rechazados.", "Dexcom-Benutzername oder -Passwort abgelehnt.",
                             "Nome utente o password Dexcom rifiutati.", "Utilizador ou palavra-passe Dexcom recusados."},
[S_SETUP_FAIL_DEX_LOCK] = {"Compte bloqué par Dexcom après trop d'essais. Attendez un moment avant de réessayer.",
                             "Account locked by Dexcom after too many attempts. Wait a while before retrying.",
                             "Cuenta bloqueada por Dexcom tras demasiados intentos. Espere antes de reintentar.",
                             "Konto von Dexcom nach zu vielen Versuchen gesperrt. Warten Sie, bevor Sie es erneut versuchen.",
                             "Account bloccato da Dexcom dopo troppi tentativi. Attendi prima di riprovare.",
                             "Conta bloqueada pela Dexcom após demasiadas tentativas. Aguarde antes de tentar de novo."},
[S_SETUP_FAIL_DEX_NET] = {"Serveur Dexcom injoignable pour l'instant.", "Dexcom server unreachable right now.",
                             "Servidor Dexcom inaccesible por ahora.", "Dexcom-Server derzeit nicht erreichbar.",
                             "Server Dexcom non raggiungibile al momento.", "Servidor Dexcom inacessível de momento."},
[S_SETUP_FAIL_OTHER] = {"Échec inattendu. Réessayez.", "Unexpected failure. Try again.",
                             "Fallo inesperado. Inténtelo de nuevo.", "Unerwarteter Fehler. Erneut versuchen.",
                             "Errore imprevisto. Riprova.", "Falha inesperada. Tente de novo."},
[S_SETUP_RETRY] = {"Réessayer", "Retry", "Reintentar", "Erneut versuchen", "Riprova", "Tentar de novo"},
[S_SETUP_BACK] = {"Retour", "Back", "Atrás", "Zurück", "Indietro", "Voltar"},
[S_SETUP_NEXT] = {"Suivant", "Next", "Siguiente", "Weiter", "Avanti", "Seguinte"},
[S_SETUP_FINISH] = {"Terminer", "Finish", "Terminar", "Fertig", "Fine", "Terminar"},
[S_SETUP_DEMO_NOTE] = {"Mode démonstration : les valeurs sont fictives, aucun compte n'est utilisé.",
                             "Demo mode: values are made up, no account is used.",
                             "Modo demostración: los valores son ficticios, no se usa ninguna cuenta.",
                             "Demo-Modus: die Werte sind erfunden, es wird kein Konto verwendet.",
                             "Modalità dimostrativa: i valori sono fittizi, nessun account viene usato.",
                             "Modo demonstração: os valores são fictícios, nenhuma conta é usada."},
[S_SETUP_EMPTY] = {"À renseigner", "Required", "Falta rellenar", "Bitte ausfüllen", "Da compilare", "Por preencher"},
/* --- Section CONNEXION adjustments ---------------------------------------------------------------------------- */
[S_CONN_TITLE] = {"CONNEXION", "CONNECTION", "CONEXIÓN", "VERBINDUNG", "CONNESSIONE", "LIGAÇÃO"},
[S_CONN_WIFI] = {"Réseau Wi-Fi", "Wi-Fi network", "Red Wi-Fi", "WLAN-Netz", "Rete Wi-Fi", "Rede Wi-Fi"},
[S_CONN_DEXCOM] = {"Compte Dexcom", "Dexcom account", "Cuenta Dexcom", "Dexcom-Konto", "Account Dexcom", "Conta Dexcom"},
[S_CONN_MODIFY] = {"Modifier", "Change", "Modificar", "Ändern", "Modifica", "Alterar"},
[S_CONN_ERASE] = {"Effacer", "Erase", "Borrar", "Löschen", "Cancella", "Apagar"},
[S_CONN_ERASE_ASK] = {"Effacer les identifiants ?", "Erase the credentials?", "¿Borrar las credenciales?",
                             "Zugangsdaten löschen?", "Cancellare le credenziali?", "Apagar as credenciais?"},
[S_CONN_ERASE_HELP] = {"Le réseau Wi-Fi et le compte Dexcom sont effacés. L'appareil redémarre et redemande la configuration.",
                             "The Wi-Fi network and the Dexcom account are erased. The device restarts and asks again.",
                             "Se borran la red Wi-Fi y la cuenta Dexcom. El aparato se reinicia y vuelve a preguntar.",
                             "WLAN und Dexcom-Konto werden gelöscht. Das Gerät startet neu und fragt erneut.",
                             "La rete Wi-Fi e l'account Dexcom vengono cancellati. L'apparecchio si riavvia e richiede la configurazione.",
                             "A rede Wi-Fi e a conta Dexcom são apagadas. O aparelho reinicia e pergunta de novo."},
[S_CONN_DEMO] = {"Mode démonstration", "Demo mode", "Modo demostración", "Demo-Modus", "Modalità dimostrativa", "Modo demonstração"},
[S_CONN_HELP] = {"Les identifiants restent dans cet appareil. Ils ne sont jamais affichés ni écrits dans les journaux.",
                             "Credentials stay in this device. They are never displayed nor written to the logs.",
                             "Las credenciales se quedan en este aparato. Nunca se muestran ni se escriben en los registros.",
                             "Die Zugangsdaten bleiben im Gerät. Sie werden nie angezeigt oder protokolliert.",
                             "Le credenziali restano in questo apparecchio. Non sono mai mostrate né scritte nei registri.",
                             "As credenciais ficam neste aparelho. Nunca são mostradas nem escritas nos registos."},
[S_CONN_NOT_SET] = {"non renseigné", "not set", "sin configurar", "nicht gesetzt", "non impostato", "por definir"},
[S_SETUP_CONFIRM] = {"Enregistrer ces accès ? Les précédents sont conservés jusque-là.",
                             "Save these credentials? The previous ones are kept until then.",
                             "¿Guardar estos accesos? Los anteriores se conservan hasta entonces.",
                             "Diese Zugangsdaten speichern? Bis dahin bleiben die vorherigen erhalten.",
                             "Salvare questi accessi? I precedenti restano fino ad allora.",
                             "Guardar estes acessos? Os anteriores são mantidos até então."},
[S_SETUP_KEEP_OLD] = {"Garder les anciens", "Keep the old ones", "Mantener los anteriores",
                             "Alte behalten", "Tieni i precedenti", "Manter os anteriores"},
[S_SETUP_KEPT_OLD] = {"Vos accès précédents sont conservés : rien n'a été modifié.",
                             "Your previous credentials are kept: nothing has been changed.",
                             "Sus accesos anteriores se conservan: no se ha cambiado nada.",
                             "Ihre bisherigen Zugangsdaten bleiben erhalten: nichts wurde geändert.",
                             "I tuoi accessi precedenti sono conservati: nulla è stato modificato.",
                             "Os seus acessos anteriores são mantidos: nada foi alterado."},
[S_CONN_SAVE] = {"Enregistrer", "Save", "Guardar", "Speichern", "Salva", "Guardar"},
[S_CONN_TRIAL] = {"Essai en cours : accès pas encore enregistrés.",
                             "Trial in progress: credentials not saved yet.",
                             "Prueba en curso: accesos aún no guardados.",
                             "Test läuft: Zugangsdaten noch nicht gespeichert.",
                             "Prova in corso: accessi non ancora salvati.",
                             "Ensaio em curso: acessos ainda não guardados."},
};

const char *tr(str_id_t id)
{
    if (id < 0 || id >= S_COUNT) {
        return "";
    }
    const char *s = k_tr[id][s_lang];
    /* Replies to the French if a translation is missing: never empty text on screen. */
    return (s != NULL && s[0] != '\0') ? s : k_tr[id][LANG_FR];
}

const char *tr_fr(str_id_t id)
{
    return (id >= 0 && id < S_COUNT && k_tr[id][LANG_FR] != NULL) ? k_tr[id][LANG_FR] : "";
}

/* ------------------------------------------------------------------------------------------------
 * Dates longues
 * ---------------------------------------------------------------------------------------------- */

static const char *const k_days[LANG_COUNT][7] = {
    {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"},
    {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"},
    {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"},
    {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"},
    {"domenica", "lunedì", "martedì", "mercoledì", "giovedì", "venerdì", "sabato"},
    {"domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira", "sábado"},
};

static const char *const k_months[LANG_COUNT][12] = {
    {"janvier", "février", "mars", "avril", "mai", "juin",
     "juillet", "août", "septembre", "octobre", "novembre", "décembre"},
    {"January", "February", "March", "April", "May", "June",
     "July", "August", "September", "October", "November", "December"},
    {"enero", "febrero", "marzo", "abril", "mayo", "junio",
     "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"},
    {"Januar", "Februar", "März", "April", "Mai", "Juni",
     "Juli", "August", "September", "Oktober", "November", "Dezember"},
    {"gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno",
     "luglio", "agosto", "settembre", "ottobre", "novembre", "dicembre"},
    {"janeiro", "fevereiro", "março", "abril", "maio", "junho",
     "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"},
};

/* Mise en forme propre a chaque langue : "Jeudi 15 octobre", "Thursday 15 October",
 * "jueves, 15 de octubre", "Donnerstag, 15. Oktober", "giovedì 15 ottobre",
 * "quinta-feira, 15 de outubro." The French keep "1er" for the first of the month. */
const char *i18n_day_name(int wday)
{
    return k_days[s_lang][(wday >= 0 && wday < 7) ? wday : 0];
}

/* Produce localized day and month names for the selected language. */
void i18n_format_date_long(const struct tm *tm_local, char *buf, size_t size)
{
    const int wday = (tm_local->tm_wday >= 0 && tm_local->tm_wday < 7) ? tm_local->tm_wday : 0;
    const int mon = (tm_local->tm_mon >= 0 && tm_local->tm_mon < 12) ? tm_local->tm_mon : 0;
    const char *jour = k_days[s_lang][wday];
    const char *mois = k_months[s_lang][mon];
    const int d = tm_local->tm_mday;

    switch (s_lang) {
    case LANG_EN:
        snprintf(buf, size, "%s %d %s", jour, d, mois);
        break;
    case LANG_ES:
        snprintf(buf, size, "%s, %d de %s", jour, d, mois);
        break;
    case LANG_DE:
        snprintf(buf, size, "%s, %d. %s", jour, d, mois);
        break;
    case LANG_IT:
        snprintf(buf, size, "%s %d %s", jour, d, mois);
        break;
    case LANG_PT:
        snprintf(buf, size, "%s, %d de %s", jour, d, mois);
        break;
    case LANG_FR:
    default:
        if (d == 1) {
            snprintf(buf, size, "%s 1er %s", jour, mois);
        } else {
            snprintf(buf, size, "%s %d %s", jour, d, mois);
        }
        break;
    }
    /* Spanish, Italian and Portuguese: capital in headband, like other languages. */
    if (buf[0] >= 'a' && buf[0] <= 'z') {
        buf[0] = (char)(buf[0] - 'a' + 'A');
    }
}
