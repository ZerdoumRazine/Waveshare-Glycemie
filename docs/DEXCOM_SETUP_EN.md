# Dexcom Share Configuration

The device reads the measurements via **Dexcom Share**, the service that allows a person
followed by sharing blood sugar with relatives. It never dialogues with the sensor, and
does not write **never** anything on Dexcom: all queries are read.

Complete chain: G7 sensor → Phone Dexcom app → Dexcom servers Share →
Internet → Wi-Fi → screen.

---

## 1. Dexcom, first and foremost

1. In the Dexcom app of the phone carrying the sensor, activate **Share**
   ("Share") and create at least one subscriber ("Follower"). The division must be **active**:
   Without a subscriber, the Share service does not publish anything.
2. The phone must remain connected to the Internet: it is it that pushes the measurements to
   Dexcom. If the phone is offline, the screen will display "Data ANCIENNES ».

## 2. What account to use

> The firmware authenticates with the account **which shares** (the *publisher*), i.e.
> the person carrying the sensor — **not** a subscriber account.

**Recommended method (K22) :** on the screen, open **REGALS → CONNEXION**
or follow the assistant that appears at the first boot. Seizing Your Own
account Dexcom Share and choose the region. Connection is tried before
confirmation; IDs need not be included in the code.

A Windows application `GlycemieConnexion.exe` also allows
cable configuration USB.

**Former developer method only:** Custom firmware
can incorporate values into `private/device_secrets.h`, off deposit.
**Never distribute the binary** of such firmware.

## 3. Region

| Region | Value | Server |
|---|---|---|
| Europe / outside the United States | `"ous"` | `shareous1.dexcom.com` |
| United States | `"us"` | `share2.dexcom.com` |

**Only the region `ous` was actually used and validated.** Code accepts `us`but
This path has never been tried on an American account.

Application ID used: `d89443d2-327c-4a6f-89e5-496bbb0317db`, the
*pydexcom* (`DEXCOM_APPLICATION_ID_OUS = DEXCOM_APPLICATION_ID_US`). Identifier
`d8665ade-…` in former projects Arduino is the one of **Japan**:
Not working here.

## 4. Authentication lock — read before the first start

Dexcom temporarily blocks an account after several authentication failures. For
a bug or a restart loop never locks the account, `main/dexcom_guard.c`
recorded in NVS status of authentication, linked to a ** short footprint** of
identifiers (never the identifiers themselves):

| State | Behaviour |
|---|---|
| **INCONNU** | no authentication until a single attempt is allowed |
| **TENTATIVE EN COURS** | recorded *before* the consignment; after a restart, no new attempt |
| **VALIDE** | Normal operation: connection to boot, automatic session renewal |
| **REFUSE** | refused IDs or blocked account: no more attempts, even after restarting |
| **NON CONCLUANT** | identifiers sent without verdict (network or server): more automatic attempt |

**With the assistant K22**, the user initiates an attempt to
connection and can confirm the recording when the test succeeds:
the console is **not required** for normal installation.

**On the old compilation method**, the authentication lock
may require manual authorization. The console control of
Historical diagnosis is:

```powershell
python tools\serial_capture.py --port COM7 --duration 60 --send "3:dexcom_status" --send "6:dexcom_auth" --log dexcom.log
```

- `dexcom_status` displays the current state.
- `dexcom_auth` allows **an** attempt, only if the state is INCONNU.

If the request never left the card (DNS, TCP ou TLS in failure), it is not counted
as an attempt: the state returns to INCONNU. Change IDs in
`device_secrets.h` hands over state to INCONNU (a new authorisation will be required).

## 5. What the device reads

Three calls from theAPI Share, all read:

1. `AuthenticatePublisherAccount`
2. `LoginPublisherAccountById`
3. `ReadPublisherLatestGlucoseValues`

Fields used: `WT` (time stamping of the measurement), `ST`, `DT`, `Value` (mg/dL), `Trend`.

Reading policy (24 h cache in PSRAMup to 288 measures, merger without duplication):

- on start: 24 full hours (1,440 min);
- next: short reading at each new measurement expected (last measurement + 5 min 20 s),
  never less than 30 s between two requests;
- catch up with the last 3 hours every hour;
- complete review of 24 hours every 6 hours;
- Pages GRAPHIQUE et AUJOURD'HUI *Never** Dexcom: they read the cache.

In case of network failure or expired session, the reconnection is automatic (observed in
6 seconds after a voluntary Wi-Fi switch). Missed measurements are caught on return,
without duplicate in history CSV.

## 6. Journalization and privacy

- No ID, password, session token or UUID is written in newspapers,
  on the microSD or on the screen. Dexcom error messages are logged with the step,
  code HTTP and Dexcom code, the rest being **masked**.
- `tools/serial_capture.py` mask in addition, on the fly, any value present in
  `private/device_secrets.h` and all UUIDBefore writing the paper.
- Le SSID Wi-Fi is not updated either.

## 7. Without account Dexcom

The demo mode allows you to install, try and present the device without any
account: it generates 24 hours of data **fictitious** in memory, with a banner " MODE DEMO" to
the screen, and does not write anything on the microSD.

```
demo normal      courbe réaliste autour de 150 mg/dL
demo low         descente vers 64
demo verylow     descente vers 48 (déclenche l'alerte très basse)
demo high        montée vers 265
demo veryhigh    montée vers 330
demo gap         trou d'une heure dans l'historique
demo stale       dernière mesure vieille de 18 min (DONNÉES ANCIENNES)
demo sparse      10 h de mesures sur 24 h (couverture insuffisante)
demo off         retour aux données réelles (les données simulées sont effacées)
```

## 8. Warning

Dexcom Share is **not** a API documented. This project is based on the
observed behavior and on the work of *pydexcom*: a Dexcom side change can
Discontinue service without notice. This is one of the reasons why this display
must never replace the official alarms of the sensor and telephone.
