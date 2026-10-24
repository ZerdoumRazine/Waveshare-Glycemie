# Dexcom Share configuration and data handling

WaveShare Glycémie reads glucose data through **Dexcom Share**, not directly from the G7 sensor. The data path is G7 sensor → official Dexcom phone application → Dexcom Share servers → Internet → Waveshare display. The device sends no commands to the sensor and does not change Dexcom data.

## Before configuring the display

1. In the official Dexcom phone application, enable **Share** and ensure it has an active follower. The phone must be able to upload readings to Dexcom.
2. Keep the phone and display connected to the Internet. When uploads stop, the display must show the age of its last reading rather than implying it is current.
3. Use the **publisher account** associated with the person sharing the readings, not a follower's account.

## Enter your own credentials

For the public firmware, select **Settings → Connection** on the touchscreen or complete the first-run wizard. Enter your Wi-Fi password, Dexcom Share account and region. The wizard performs a trial connection; it saves the replacement credentials only after a successful result and explicit confirmation.

The alternative `GlycemieConnexion.exe` Windows application uses the USB serial cable. No source code edit or rebuild is required. The historical `private/device_secrets.h` method is only for local private builds; never publish a resulting binary or flash backup.

## Account regions

| Account region | Code | Share endpoint |
|---|---|---|
| Europe / outside the US | `ous` | `shareous1.dexcom.com` |
| United States | `us` | `share2.dexcom.com` |

Only the **OUS** region was exercised with a real account during this project's original development. Other regions exist in the code but should not be described as tested on real accounts. The application identifiers and protocol paths follow established Dexcom Share client behavior, including the third-party references documented in [Third-party licences](THIRD_PARTY_LICENSES_EN.md).

## Why authentication has a persistent guard

Repeated rejected login attempts can lock a Dexcom account. `main/dexcom_guard.c` maintains a persistent attempt state linked to a short fingerprint of the selected credentials. It distinguishes unknown, in-progress, validated, refused and inconclusive attempts.

The firmware records that it is about to attempt a login **before** sending the network request. If a restart interrupts an uncertain attempt, the device does not blindly retry in a loop. With the current setup wizard, the user explicitly initiates a test; the older serial diagnostic path used `dexcom_status` and `dexcom_auth` commands. Do not repeat login attempts rapidly when a password or session is rejected.

## Read-only protocol and refresh policy

The client performs the Share calls `AuthenticatePublisherAccount`, `LoginPublisherAccountById`, and `ReadPublisherLatestGlucoseValues`. The display uses reading timestamps and fields such as `Value` and `Trend`; it does not invent measurements in gaps.

The implementation's historical refresh policy includes a full 24-hour history request on startup, short reads around expected new measurements, hourly catch-up of recent history, and periodic full-history refreshes. The 24-hour in-memory cache can hold up to **288 readings**. UI pages draw from this cache instead of making separate network calls.

Connection failures, expired sessions, and missing data must remain distinguishable from a valid recent glucose reading. Catch-up and deduplication aim to avoid duplicate CSV history rows.

## Privacy and demo mode

Access tokens, passwords and account credentials should not appear in logs or screenshots. The USB capture tool attempts to redact known secrets; this is a safeguard, **not a guarantee** that every sensitive value has been found. Keep serial logs and full-device backups private. NVS credential obfuscation is reversible and is **not encryption**.

Demo mode uses fictitious history without a Dexcom account. Supported serial scenarios include `demo normal`, `demo low`, `demo verylow`, `demo high`, `demo veryhigh`, `demo gap`, `demo stale`, `demo sparse`, and `demo off`. Use these for demonstrations and UI checks, never for clinical decisions.

## Reliability and medical limitations

Dexcom Share is an undocumented service interface and may change without notice. The availability of a network connection does not establish the safety or freshness of a reading. WaveShare Glycémie is an **experimental supplementary display**; official Dexcom monitoring and alarms remain essential, and this project must not be used to make treatment decisions.
