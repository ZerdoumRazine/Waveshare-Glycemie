/*
 * MODELE - a copier en <racine du projet>/private/device_secrets.h puis a completer.
 * Sous Windows :  copy main\device_secrets.example.h private\device_secrets.h
 *
 * Le vrai fichier ne doit JAMAIS etre publie, versionne, joint a un rapport ou copie
 * dans un journal. Les valeurs sont compilees dans le firmware : ne jamais publier
 * non plus le dossier build/ ni les binaires produits.
 */
#pragma once

#define GLY_WIFI_SSID        ""       /* reseau Wi-Fi 2,4 GHz */
#define GLY_WIFI_PASSWORD    ""
#define GLY_DEXCOM_USERNAME  ""       /* compte Dexcom QUI PARTAGE (Share), pas un abonne */
#define GLY_DEXCOM_PASSWORD  ""
#define GLY_DEXCOM_REGION    "ous"    /* Europe : shareous1.dexcom.com */
