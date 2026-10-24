/* Template for optional local credentials; never put real passwords in Git.
 * Test and configuration interface only; no live patient data is included.
 */
/*
 * MODELE - a copier en <racine du projet>/private/device_secrets.h puis a completer.
 * Sous Windows :  copy main\device_secrets.example.h private\device_secrets.h
 *
 * The real file must not JAMAIS etre publie, versionne, joint a un rapport ou copie
 * in a newspaper. Values are compiled in firmware: never publish
 * neither the build/ folder nor the binary products.
 */
/* Public interface / Interface publique
 * Example values for developers only; the public build is configured on the device.
 */
#pragma once

#define GLY_WIFI_SSID        ""       /* reseau Wi-Fi 2,4 GHz */
#define GLY_WIFI_PASSWORD    ""
#define GLY_DEXCOM_USERNAME  ""       /* compte Dexcom QUI PARTAGE (Share), pas un abonne */
#define GLY_DEXCOM_PASSWORD  ""
#define GLY_DEXCOM_REGION    "ous"    /* Europe : shareous1.dexcom.com */
