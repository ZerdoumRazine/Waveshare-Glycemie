# Checks hashes and hardware before USB firmware installation.
#
# Design note: Do not flash until the user explicitly confirms.
# Before USB flash, verify the firmware checksums, board revision and user confirmation.
#
<#
.SYNOPSIS
    Install the public firmware on Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.

.DESCRIPTION
    Require matching firmware images, SHA-256 checksums, supported chip revision
    and explicit approval. Do not erase NVS or format microSD. This installer
    must not be used on an unverified board revision.
#>
param([string]$Port = "")
$ErrorActionPreference = 'Stop'
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path

# The public firmware does not contain credentials. Configure them using
# the touchscreen wizard or GlycemieConnexion.exe after installation.
$required = @('glycemie_display.bin', 'bootloader.bin', 'partition-table.bin')
foreach ($name in $required) {
    if (-not (Test-Path (Join-Path $dir $name))) {
        throw "Fichier obligatoire absent : $name. Extrayez le ZIP entier avant installation."
    }
}

# Check file checksums before starting any flash operation.
$hashFile = Join-Path $dir 'SHA256SUMS.txt'
if (-not (Test-Path $hashFile)) {
    throw "SHA256SUMS.txt absent. L'archive doit etre extraite en entier."
}
$expected = @{}
foreach ($line in (Get-Content $hashFile)) {
    if ($line -match '^([a-fA-F0-9]{64})  (.+)$') {
        $expected[$matches[2]] = $matches[1].ToLower()
    }
}
foreach ($name in $required) {
    if (-not $expected.ContainsKey($name)) { throw "Empreinte manquante pour $name" }
    $actual = (Get-FileHash (Join-Path $dir $name) -Algorithm SHA256).Hash.ToLower()
    if ($actual -ne $expected[$name]) { throw "Fichier altere : $name. Arret sans flash." }
}
Write-Host "Integrite des trois fichiers verifiee." -ForegroundColor Green

if (Get-Command py -ErrorAction SilentlyContinue) {
    $python = "py"
    $pyArgs = @("-3", "-m", "esptool")
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $python = "python"
    $pyArgs = @("-m", "esptool")
} else {
    throw "Python est necessaire : installez-le depuis python.org."
}
& $python @pyArgs version 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "esptool est absent. Ouvrez PowerShell puis lancez : py -3 -m pip install esptool"
}

if (-not $Port) {
    $detected = @(Get-CimInstance Win32_PnPEntity | Where-Object {
        $_.Name -match '\(COM\d+\)' -and
        ($_.Name -match 'CH343' -or $_.Name -match 'USB-Enhanced-SERIAL')
    })
    if ($detected.Count -ne 1) {
        Write-Host "Detection automatique impossible : $($detected.Count) port(s) CH343." -ForegroundColor Yellow
        Write-Host "Dans le Gestionnaire de peripheriques, identifiez le port UART de votre Waveshare."
        Write-Host "Relancez : .\installer.ps1 -Port COM7  (remplacez COM7 par votre port)."
        exit 2
    }
    $Port = [regex]::Match($detected[0].Name, 'COM\d+').Value
}
if ($Port -notmatch '^COM\d+$') { throw "Port incorrect : $Port (attendu : COM7 par exemple)" }
Write-Host "Verification du port $Port et de la puce ESP32-P4..." -ForegroundColor Cyan
$probe = & $python @pyArgs --chip esp32p4 --port $Port flash_id 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "Le Waveshare ne repond pas sur $Port. Aucun fichier n'a ete ecrit."
}
$chipInfo = ($probe | Out-String)
if ($chipInfo -match 'revision\s+v?(\d+)\.(\d+)') {
    $major = [int]$matches[1]
    $minor = [int]$matches[2]
    if ($major -ge 3) {
        throw "Revision ESP32-P4 v$major.$minor non prise en charge. Aucun flash effectue."
    }
    Write-Host "Revision detectee : v$major.$minor"
} else {
    Write-Warning "Impossible de lire la revision. Ce firmware cible ESP32-P4 rev v1.3 (pre-v3)."
    throw "Verification materielle incomplete. Aucun flash effectue."
}
Write-Host "Le programme ne remplace pas les alarmes Dexcom officielles." -ForegroundColor Yellow
Write-Host "Fermez tous les logiciels qui utilisent le port serie."
$confirmation = Read-Host "Pour installer le programme sur $Port, tapez exactement INSTALLER"
if ($confirmation -cne 'INSTALLER') {
    Write-Host "Operation annulee. Rien n'a ete ecrit."
    exit 0
}

$arguments = @(
    '--chip', 'esp32p4', '--port', $Port, '--baud', '460800',
    '--before', 'default_reset', '--after', 'hard_reset',
    'write_flash', '--flash_mode', 'qio', '--flash_freq', '80m', '--flash_size', '32MB',
    '0x2000', (Join-Path $dir 'bootloader.bin'),
    '0x8000', (Join-Path $dir 'partition-table.bin'),
    '0x10000', (Join-Path $dir 'glycemie_display.bin')
)
& $python @pyArgs @arguments
if ($LASTEXITCODE -ne 0) { throw "Installation interrompue. Consultez le tutoriel de depannage." }
Write-Host "Installation terminee. Configurez vos acces sur l'ecran ou dans l'application Windows." -ForegroundColor Green
