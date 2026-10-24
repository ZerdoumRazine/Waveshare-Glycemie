/* Template for optional local credentials; never put real passwords in Git.
 * Test and configuration interface only; no live patient data is included.
 */
/* Optional local-build credentials template.
 * Copy to private/device_secrets.h only for a private development build.
 * Windows example: copy main\device_secrets.example.h private\device_secrets.h
 * Never commit, distribute or log the completed file. Compiled firmware
 * built with real secrets must also remain private.
 * Public builds accept credentials through touchscreen/USB setup instead. */
/* Empty placeholder values for a local-only development configuration. */
#pragma once

#define GLY_WIFI_SSID        ""       /* SSID for the 2.4 GHz Wi-Fi network. */
#define GLY_WIFI_PASSWORD    ""
#define GLY_DEXCOM_USERNAME  ""       /* Dexcom account that shares readings; not a follower account. */
#define GLY_DEXCOM_PASSWORD  ""
#define GLY_DEXCOM_REGION    "ous"    /* European Dexcom Share region. */
