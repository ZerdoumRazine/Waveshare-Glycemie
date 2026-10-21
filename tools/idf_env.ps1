# EN: Locate and activate the exact ESP-IDF toolchain needed by this project.
# FR: Active la chaîne d'outils ESP-IDF utilisée par le projet.
# Environnement ESP-IDF v5.5.5 pour glycemie_display.
# A dot-sourcer dans chaque session PowerShell, depuis n'importe ou :
#     . <racine du projet>\tools\idf_env.ps1
#
# Toutes les variables sont limitees au processus courant : rien n'est ecrit dans les
# variables d'environnement Windows ni dans le PATH permanent.
#
# Deux modes :
#  1. ESP-IDF installe DANS le projet (tools\esp-idf-v5.5.5 + tools\espressif) : detecte tout seul.
#  2. ESP-IDF installe ailleurs : definir IDF_PATH avant d'appeler ce script, par exemple
#         $env:IDF_PATH = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
#     ou utiliser directement le raccourci "ESP-IDF 5.5 PowerShell" fourni par l'installateur.

$ProjectRoot = Split-Path -Parent $PSScriptRoot

$LocalIdf = Join-Path $ProjectRoot 'tools\esp-idf-v5.5.5'
if (-not $env:IDF_PATH -and (Test-Path $LocalIdf)) { $env:IDF_PATH = $LocalIdf }
if (-not $env:IDF_PATH) {
    Write-Error "IDF_PATH n'est pas defini et aucun ESP-IDF n'a ete trouve dans $LocalIdf. Voir docs\INSTALLATION_FR.md."
    return
}

$LocalTools = Join-Path $ProjectRoot 'tools\espressif'
if (-not $env:IDF_TOOLS_PATH -and (Test-Path $LocalTools)) { $env:IDF_TOOLS_PATH = $LocalTools }

# Caches rediriges dans le projet : la compilation n'ecrit rien ailleurs.
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
