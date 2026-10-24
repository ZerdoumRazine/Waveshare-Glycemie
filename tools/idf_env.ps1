# Loads the ESP-IDF build environment.
#
# Design note: Keep the toolchain separate from released source files.
# Locate and activate the exact ESP-IDF toolchain needed by this project.
#
# Environnement ESP-IDF v5.5.5 for glycemie_display.
# To dot-source in each PowerShell session, from anywhere or:
#     . <racine du projet>\tools\idf_env.ps1
#
# All variables are limited to the current process: nothing is written in the
# environment variables Windows nor in the PATH permanent.
#
# Deux modes :
#  1. ESP-IDF install DANS the project (tools\esp-idf-v5.5.5 + tools\espressive): detects on its own.
#  2. ESP-IDF installe ailleurs : definir IDF_PATH before calling this script, for example
#         $env:IDF_PATH = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
#     or use the shortcut directly "ESP-IDF 5.5 PowerShell" supplied by the installer.

$ProjectRoot = Split-Path -Parent $PSScriptRoot

$LocalIdf = Join-Path $ProjectRoot 'tools\esp-idf-v5.5.5'
if (-not $env:IDF_PATH -and (Test-Path $LocalIdf)) { $env:IDF_PATH = $LocalIdf }
if (-not $env:IDF_PATH) {
    Write-Error "IDF_PATH n'est pas defini et aucun ESP-IDF n'a ete trouve dans $LocalIdf. Voir docs\INSTALLATION_FR.md."
    return
}

$LocalTools = Join-Path $ProjectRoot 'tools\espressif'
if (-not $env:IDF_TOOLS_PATH -and (Test-Path $LocalTools)) { $env:IDF_TOOLS_PATH = $LocalTools }

# Hides redirect in the project: the compilation does not write anything else.
$Cache = Join-Path $ProjectRoot 'tools\cache'
$env:IDF_COMPONENT_CACHE_PATH      = Join-Path $Cache 'components'
$env:PIP_CACHE_DIR                 = Join-Path $Cache 'pip'
$env:PIP_DISABLE_PIP_VERSION_CHECK = '1'
$env:PYTHONPYCACHEPREFIX           = Join-Path $Cache 'pycache'
$env:PYTHONUTF8                    = '1'
$env:IDF_CCACHE_ENABLE             = '0'
$env:TEMP                          = Join-Path $Cache 'tmp'
$env:TMP                           = $env:TEMP
New-Item -ItemType Directory -Force $env:TEMP | Out-Null

if ($args -notcontains '-NoExport') {
    $idf_exports = python "$env:IDF_PATH\tools\activate.py" --export
    . $idf_exports
}
