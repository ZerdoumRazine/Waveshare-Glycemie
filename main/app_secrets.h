/*
 * Acces aux identifiants prives.
 *
 * Les valeurs viennent de <racine du projet>/private/device_secrets.h (hors depot,
 * voir main/device_secrets.example.h). Ce fichier ne doit etre inclus que par les
 * modules qui en ont besoin, et aucune de ces valeurs ne doit jamais etre journalisee.
 */
/* Public interface / Interface publique
 * EN: Private-build compatibility values; never log or distribute real credentials.
 * FR: Compatibilité des identifiants locaux à ne jamais publier.
 */
#pragma once

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#define GLY_HAS_PRIVATE_SECRETS 1
#else
#define GLY_HAS_PRIVATE_SECRETS 0
#endif

#ifndef GLY_WIFI_SSID
#define GLY_WIFI_SSID ""
#endif
#ifndef GLY_WIFI_PASSWORD
#define GLY_WIFI_PASSWORD ""
#endif
#ifndef GLY_DEXCOM_USERNAME
#define GLY_DEXCOM_USERNAME ""
#endif
#ifndef GLY_DEXCOM_PASSWORD
#define GLY_DEXCOM_PASSWORD ""
#endif
#ifndef GLY_DEXCOM_REGION
#define GLY_DEXCOM_REGION "ous"
#endif
