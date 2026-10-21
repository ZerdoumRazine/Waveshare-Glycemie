# Builds firmware using the designated ESP-IDF environment.
# FR: Compile le firmware avec l'environnement ESP-IDF prévu.
# Design note: Build from clean settings to prevent stale optional voice settings.
# EN: Build firmware in a compatible ASCII-only path, with explicit voice-recognition options.
# FR: Compile le firmware depuis un chemin compatible avec ESP-IDF.
#Requires -Version 5.1
<#
.SYNOPSIS
    Compile le firmware glycemie_display.

.DESCRIPTION
    ESP-IDF ne sait pas compiler depuis un chemin contenant un caractere accentue : ccache,
    ldgen.py et l'editeur de liens echouent tous les trois. Si la racine du projet contient un
    accent, ce script recopie le projet dans un dossier de travail ASCII (sous le dossier temporaire
    de l'utilisateur, jamais a cote du projet) et compile la-bas. Sinon il compile sur place.

    Le dossier private/ n'est JAMAIS efface par la recopie et sa presence est verifiee avant de
    compiler : un firmware sans identifiants demarre mais reste sans Wi-Fi ni Dexcom.

.PARAMETER Clean
    Repart d'une compilation vide (fullclean) et vide le dossier de travail.

.PARAMETER Flash
    Flashe l'application apres une compilation reussie (idf.py -p <port> app-flash).

.PARAMETER Port
    Port serie, COM7 par defaut.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build.ps1
    powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Clean
    powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Flash -Port COM7
#>
param(
    [switch]$Clean,
    [switch]$Flash,
    [switch]$Vocal,   # recompile AVEC les commandes vocales locales (absentes par defaut)
    [string]$Port = 'COM7'
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot

function Test-Ascii([string]$text) {
    return -not ($text -match '[^\u0000-\u007F]')
}

# --- 1. Dossier de compilation -------------------------------------------------------------------
if (Test-Ascii $Root) {
    $Work = $Root
    Write-Host "Compilation sur place : $Work"
} else {
    $Work = Join-Path $env:TEMP ('wsg-build-' + [System.IO.Path]::GetFileName($Root).ToLower() -replace '[^a-z0-9\-]', '')
    Write-Host "Le chemin du projet contient un accent : ESP-IDF ne le supporte pas."
    Write-Host "Copie de travail ASCII : $Work"
    if ($Clean -and (Test-Path $Work)) { Remove-Item -LiteralPath $Work -Recurse -Force }
    New-Item -ItemType Directory -Force $Work | Out-Null
    # /XD private : la copie de travail garde ses identifiants, le miroir ne doit pas les effacer.
    $null = robocopy $Root $Work /MIR /NFL /NDL /NJH /NJS /NP `
        /XD build managed_components .git private 'tools\logs' 'tools\cache' 'tools\espressif' `
            'tools\esp-idf-v5.5.5' 'tests\build' Razine
    if ($LASTEXITCODE -ge 8) { throw "Copie de travail impossible (robocopy $LASTEXITCODE)" }
    $priv = Join-Path $Work 'private'
    New-Item -ItemType Directory -Force $priv | Out-Null
    # Les identifiants sont recopies des qu'ils different : sinon une ancienne copie de travail
    # garderait un Wi-Fi ou un compte Dexcom perime sans que rien ne le signale.
    $src = Join-Path $Root 'private\device_secrets.h'
    $dst = Join-Path $priv 'device_secrets.h'
    if (Test-Path $src) {
        $memeContenu = (Test-Path $dst) -and
            ((Get-FileHash $src -Algorithm SHA256).Hash -eq (Get-FileHash $dst -Algorithm SHA256).Hash)
        if (-not $memeContenu) {
            Copy-Item $src $dst -Force
            Write-Host "Identifiants prives mis a jour dans la copie de travail."
        }
    }
}

# --- 2. Identifiants ------------------------------------------------------------------------------
$secrets = Join-Path $Work 'private\device_secrets.h'
if (-not (Test-Path $secrets)) {
    Write-Warning "private\device_secrets.h est absent."
    Write-Warning "Le firmware compilera, mais il n'aura ni Wi-Fi ni compte Dexcom :"
    Write-Warning "seul le mode demonstration fonctionnera. Voir docs\INSTALLATION_FR.md."
    Write-Warning "Ctrl+C pour arreter, ou patientez 10 s pour compiler quand meme."
    Start-Sleep -Seconds 10
}

# --- 3. Environnement ESP-IDF ---------------------------------------------------------------------
if (-not $env:IDF_PATH) {
    $local = Join-Path $Root 'tools\esp-idf-v5.5.5'
    if (Test-Path $local) { $env:IDF_PATH = $local }
}
if (-not $env:IDF_PATH) {
    throw "IDF_PATH n'est pas defini. Ouvrez la fenetre 'ESP-IDF 5.5 PowerShell', ou definissez `$env:IDF_PATH. Voir docs\INSTALLATION_FR.md."
}
if (-not $env:IDF_TOOLS_PATH) {
    # Installation « dans le projet » : les outils sont a cote d'ESP-IDF (tools\espressif).
    foreach ($candidat in @((Join-Path (Split-Path -Parent $env:IDF_PATH) 'espressif'),
                            (Join-Path $Root 'tools\espressif'))) {
        if (Test-Path (Join-Path $candidat 'python_env')) { $env:IDF_TOOLS_PATH = $candidat; break }
    }
}
$env:PYTHONUTF8 = '1'
# IDF_CCACHE_ENABLE est teste non vide par ESP-IDF : "0" l'ACTIVE. Il faut supprimer la variable.
Remove-Item Env:IDF_CCACHE_ENABLE -ErrorAction SilentlyContinue

# activate.py ecrit sa progression sur stderr : sans cela, ErrorActionPreference = 'Stop'
# l'interpreterait comme une erreur alors que tout va bien.
$previous = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$exports = python "$env:IDF_PATH\tools\activate.py" --export
$activated = $LASTEXITCODE -eq 0
$ErrorActionPreference = $previous
if (-not $activated) { throw "Activation d'ESP-IDF impossible (IDF_PATH = $env:IDF_PATH)." }
. ([scriptblock]::Create(($exports -join "`n")))
Remove-Item Env:IDF_CCACHE_ENABLE -ErrorAction SilentlyContinue

$idfPython = Join-Path $env:IDF_TOOLS_PATH 'python_env\idf5.5_py3.12_env\Scripts\python.exe'
if (-not (Test-Path $idfPython)) { $idfPython = 'python' }

# --- 4. Compilation --------------------------------------------------------------------------------
Push-Location $Work
try {
    if ($Clean) { & $idfPython "$env:IDF_PATH\tools\idf.py" fullclean *>&1 | Out-Null }
    $logDir = Join-Path $Root 'tools\logs'
    New-Item -ItemType Directory -Force $logDir | Out-Null
    $log = Join-Path $logDir ('build_{0:yyyyMMdd_HHmmss}.log' -f (Get-Date))
    # Commandes vocales locales : absentes par defaut (voir main/CMakeLists.txt).
    # La valeur est TOUJOURS passee : CMake garde en cache ce qui lui a ete donne une fois, donc
    # une compilation faite apres un -Vocal garderait les commandes vocales sans rien dire.
    $defines = @(if ($Vocal) { '-DGLY_VOICE_CMD=ON' } else { '-DGLY_VOICE_CMD=OFF' })
    & $idfPython "$env:IDF_PATH\tools\idf.py" @defines build *>&1 | Tee-Object -FilePath $log | Out-Null
    $code = $LASTEXITCODE
    Select-String -Path $log -Pattern 'error:|FAILED:|binary size' |
        ForEach-Object { ($_.Line -replace [regex]::Escape($Work), '.') } | Select-Object -Unique
    if ($code -ne 0) { throw "Compilation en echec (voir $log)" }
    Write-Host "Compilation reussie. Journal : $log"

    if ($Flash) {
        & $idfPython "$Work\tools\verif_avant_flash.py"
        if ($LASTEXITCODE -ne 0) { throw "Verifications avant flash en echec : rien n'a ete flashe." }
        & $idfPython "$env:IDF_PATH\tools\idf.py" -p $Port app-flash
        if ($LASTEXITCODE -ne 0) { throw "Flash en echec." }
        Write-Host "Flash termine sur $Port (NVS conservee)."
    }
} finally {
    Pop-Location
}
