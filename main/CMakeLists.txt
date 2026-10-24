# Firmware compilation sources, options and optional voice-command stubs.
#
# Revision: refuse any resolute configuration incompatible with theESP32-P4 rev1.3.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    if(NOT CONFIG_ESP32P4_SELECTS_REV_LESS_V3 OR NOT CONFIG_ESP32P4_REV_MIN_100 OR CONFIG_ESP32P4_REV_MIN_300)
        message(FATAL_ERROR
            "sdkconfig incompatible avec l'ESP32-P4 rev1.3 : le profil pre-v3 "
            "(CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y, CONFIG_ESP32P4_REV_MIN_100=y) est obligatoire.")
    endif()
endif()

# Identifiants prives, hors du depot : <racine du projet>/private/device_secrets.h
# (dossier ignored by Git; model in main/device_secrets.example.h).
get_filename_component(GLY_PRIVATE_DIR "${CMAKE_CURRENT_LIST_DIR}/../private" ABSOLUTE)
set(GLY_PRIVATE_INCLUDE "")
if(EXISTS "${GLY_PRIVATE_DIR}/device_secrets.h")
    set(GLY_PRIVATE_INCLUDE "${GLY_PRIVATE_DIR}")
endif()

# Local voice commands (ESP-SR): ABSENTES By default. Recognition n'existe qu'in English and
# Chinese: the French phrases approached were untimely ads, and none of the six
# interface languages has no model. Spoken ads and sound alerts are not concerned.
# To reactivate them: cmake -DGLY_VOICE_CMD=ON (ou tools\build.ps1 -Vocal).
if(NOT DEFINED GLY_VOICE_CMD)
    set(GLY_VOICE_CMD OFF)
endif()
if(GLY_VOICE_CMD)
    set(GLY_VOICE_CMD_SRC "voice_cmd.c")
    set(GLY_VOICE_CMD_FLAG 1)
else()
    set(GLY_VOICE_CMD_SRC "voice_cmd_stub.c")
    set(GLY_VOICE_CMD_FLAG 0)
endif()

# Embedded fonts + old Windows voice only if it is present (see below)
set(GLY_EMBED_FILES "fonts/Montserrat-Medium.ttf" "fonts/Montserrat-Bold.ttf")
set(GLY_VOICE_OLD_PCM 0)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/voice/voice_fr.pcm")
    list(APPEND GLY_EMBED_FILES "voice/voice_fr.pcm")
    set(GLY_VOICE_OLD_PCM 1)
endif()

idf_component_register(
    SRCS
        "app_main.c"
        "app_state.c"
        "credentials.c"
        "wifi_manager.c"
        "net_services.c"
        "dexcom_share.c"
        "dexcom_guard.c"
        "i18n.c"
        "glucose_math.c"
        "glucose_service.c"
        "sensor_session.c"
        "consumables.c"
        "stock.c"
        "sd_storage.c"
        "audio_out.c"
        "settings.c"
        "alerts.c"
        "voice.c"
        "voice_rx.c"
        ${GLY_VOICE_CMD_SRC}
        "night_mode.c"
        "ui_alert.c"
        "touch_diag.c"
        "trend_arrow.c"
        "ui_common.c"
        "ui_chart.c"
        "ui_main.c"
        "ui_home.c"
        "ui_graph.c"
        "ui_today.c"
        "ui_consumables.c"
        "ui_settings.c"
        "ui_setup.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS ${GLY_PRIVATE_INCLUDE}
    EMBED_FILES ${GLY_EMBED_FILES}
)

# Former voice Windows Embedded (console "voice_old") : optional. If main/voice/voice_fr.pcm is absent
# (redistribution rights not established), the compilation succeeds and the command simply responds that it is
# Not available. Normal ads come from the microSD bank and do not depend on this file.
target_compile_definitions(${COMPONENT_LIB} PRIVATE GLY_VOICE_OLD_PCM=${GLY_VOICE_OLD_PCM}
                                                    GLY_VOICE_CMD=${GLY_VOICE_CMD_FLAG})
