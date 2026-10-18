# Afficheur familial de glycemie Dexcom G7 - Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
# ESP-IDF v5.5.5, BSP Waveshare 3.0.1, LVGL 9.5.0, ESP-Hosted (ESP32-C6).
cmake_minimum_required(VERSION 3.16)

# Profil de revision ESP32-P4 : la carte est une rev1.3 (pre-v3).
# Le profil officiel Waveshare est ajoute EN DERNIER pour primer sur les defaults du projet.
# main/CMakeLists.txt refuse en plus toute configuration resolue rev3.x.
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/config/esp32p4_rev1_3.defaults"
)

set(PROJECT_VER "0.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(glycemie_display)
