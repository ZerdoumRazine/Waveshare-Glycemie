/* app_secrets.h: Optional private-build secret integration.
 * Public distribution must never embed a user's credentials.
 */
/*
 * Acces aux identifiants prives.
 *
 * Values come from < project root>/private/device_secrets.h (hors depot,
 * voir main/device_secrets.example.h). This file should only be included by
 * modules that need it, and none of these values should ever be logged.
 */
/* Public interface / Interface publique
 * Private-build compatibility values; never log or distribute real credentials.
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
