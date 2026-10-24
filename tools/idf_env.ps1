# Loads the ESP-IDF build environment.
#
# Design note: Keep the toolchain separate from released source files.
# Locate and activate the exact ESP-IDF toolchain needed by this project.
#
# Load ESP-IDF 5.5.5 into the current PowerShell session:
#     . <project-root>\tools\idf_env.ps1
# The script does not persistently change the user's Windows PATH.
# Use a locally installed ESP-IDF at tools\esp-idf-v5.5.5, or set IDF_PATH
# to a different compatible installation before running it.
#
# Example:
#     $env:IDF_PATH = 'C:\Espressif\frameworks\esp-idf-v5.5.5'

$ProjectRoot = Split-Path -Parent $PSScriptRoot

$LocalIdf = Join-Path $ProjectRoot 'tools\esp-idf-v5.5.5'
if (-not $env:IDF_PATH -and (Test-Path $LocalIdf)) { $env:IDF_PATH = $LocalIdf }
if (-not $env:IDF_PATH) {
    Write-Error "IDF_PATH n'est pas defini et aucun ESP-IDF n'a ete trouve dans $LocalIdf. Voir docs\INSTALLATION_EN.md."
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
