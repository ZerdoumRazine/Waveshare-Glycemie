# Stages public firmware and tools apart from personal builds.
#
# Design note: Never ship a build containing Wi-Fi or Dexcom secrets.
# Legacy local firmware packaging route; never ship an image containing personal secrets.
#
<#
.SYNOPSIS
    Prepare files to download: program PUBLIC de l'ecran + application Windows + empreintes.

.DESCRIPTION
    Le programme est compile depuis la COPIE PUBLIQUE (dossier Razine), qui ne contient aucun
    Identifier: neither Wi-Fi nor Dexcom account. The user will enter his own after
    installation, on touch screen or with Windows application.

    Never compile the program from the personal work folder: this one contains your
    identifiants (private\device_secrets.h is copied for your own device).

    Result in delivery\ (outside deposit):
        glycemie_display.bin, bootloader.bin, partition-table.bin   le programme
        GlycemieConnexion.exe                                        l'application Windows
        install. ps1 installation in one control
        SHA256SUMS.txt                                               empreintes de tout

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\publier_firmware.ps1
#>
param(
    [switch]$SansExe,     # ne pas refabriquer l'application Windows
    [switch]$Propre       # recompiler le firmware de zero
)
$ErrorActionPreference = 'Stop'

$racine = Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..')
$publique = Join-Path $racine 'Razine'
$livraison = Join-Path $racine 'livraison'

if (-not (Test-Path (Join-Path $publique 'main\app_main.c'))) {
    throw "La copie publique est absente ou incomplete. Lancez d'abord :  python tools\preparer_razine.py"
}
$secretsPublics = Join-Path $publique 'private\device_secrets.h'
if (Test-Path $secretsPublics) {
    throw "ARRET : $secretsPublics existe. La copie publique ne doit contenir aucun identifiant."
}

# --- 1. Programme de l'ecran, compile depuis la copie publique ------------------------------------
Write-Host 'Compilation du programme public (sans identifiants, sans commandes vocales)...' -ForegroundColor Cyan
$build = Join-Path $publique 'tools\build.ps1'
$arguments = @('-ExecutionPolicy', 'Bypass', '-File', $build)
if ($Propre) { $arguments += '-Clean' }
& powershell @arguments
if ($LASTEXITCODE -ne 0) { throw "La compilation a echoue (code $LASTEXITCODE)." }

# build.ps1 compiles in a working copy ASCII quand le chemin contient un accent
# The outsider imposes an array: without it, a single result would be a chain,
# et $sorties[0] renverrait sa premiere LETTRE.
$sorties = @(@(
    (Join-Path $publique 'build'),
    (Join-Path $env:TEMP 'wsg-build-razine\build')
) | Where-Object { Test-Path (Join-Path $_ 'glycemie_display.bin') })
if ($sorties.Count -eq 0) { throw "glycemie_display.bin est introuvable apres la compilation." }
$build_dir = $sorties[0]
Write-Host "Programme compile : $build_dir"

New-Item -ItemType Directory -Force $livraison | Out-Null
Get-ChildItem $livraison -File | Remove-Item -Force
$aCopier = @(
    @{ src = (Join-Path $build_dir 'glycemie_display.bin'); dst = 'glycemie_display.bin' },
    @{ src = (Join-Path $build_dir 'bootloader\bootloader.bin'); dst = 'bootloader.bin' },
    @{ src = (Join-Path $build_dir 'partition_table\partition-table.bin'); dst = 'partition-table.bin' }
)
foreach ($f in $aCopier) {
    if (-not (Test-Path $f.src)) { throw "Fichier attendu absent : $($f.src)" }
    Copy-Item $f.src (Join-Path $livraison $f.dst) -Force
}

# --- 2. Application Windows -----------------------------------------------------------------------
$exe = Join-Path $racine 'tools\pc_app\dist\GlycemieConnexion.exe'
if (-not $SansExe) {
    Write-Host "Construction de l'application Windows..." -ForegroundColor Cyan
    & powershell -ExecutionPolicy Bypass -File (Join-Path $racine 'tools\pc_app\construire_exe.ps1')
    if ($LASTEXITCODE -ne 0) { throw "La construction de l'application a echoue." }
}
if (Test-Path $exe) { Copy-Item $exe $livraison -Force } else { Write-Warning "Application absente : $exe" }

# --- 3. Installation in one order, for the person who charges ------------------------------
@'
<#
    Installe le programme WaveShare Glycemie sur l'ecran, par le cable USB.

    Before: Connect the screen to the PC with a cable USB-C de DONNEES, et fermez tout moniteur serie.
    Then: right click on this file -> Run with PowerShell. Count two minutes.

    Ce script n'efface pas la carte (pas d'erase_flash) : it replaces the program, the table of
    partitions and primer. Adjustments, consumable dates and microSD are preserved.
#>
param([string]$Port = '')
$ErrorActionPreference = 'Stop'
$ici = Split-Path -Parent $MyInvocation.MyCommand.Path

function Trouver-Esptool {
    foreach ($c in @(@('esptool'), @('python', '-m', 'esptool'), @('py', '-3', '-m', 'esptool'))) {
        try { & $c[0] @($c[1..($c.Count-1)] + 'version') *> $null; if ($LASTEXITCODE -eq 0) { return $c } } catch { }
    }
    return $null
}
$esptool = Trouver-Esptool
if (-not $esptool) {
    Write-Host "esptool est necessaire. Installez Python (python.org) puis lancez :" -ForegroundColor Yellow
    Write-Host "    py -3 -m pip install esptool"
    exit 2
}
if (-not $Port) {
    $ports = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -like '*(COM*' }
    if (-not $ports) { Write-Host "Aucun port serie : branchez l'ecran, attendez deux secondes." -ForegroundColor Yellow; exit 1 }
    if ($ports.Count -eq 1 -or $ports -isnot [array]) {
        $Port = [regex]::Match($ports[0].Name, '\(COM(\d+)\)').Value.Trim('(', ')')
    } else {
        Write-Host 'Plusieurs appareils sont branches. Relancez en indiquant le bon :' -ForegroundColor Yellow
        $ports | ForEach-Object { Write-Host "   $($_.Name)" }
        Write-Host "    .\installer.ps1 -Port COM7"
        exit 1
    }
}
Write-Host "Installation sur $Port..." -ForegroundColor Cyan
$args = @('--port', $Port, '--baud', '460800', '--before', 'default_reset', '--after', 'hard_reset',
          'write_flash', '--flash_mode', 'qio', '--flash_freq', '80m', '--flash_size', '32MB',
          '0x2000', (Join-Path $ici 'bootloader.bin'),
          '0x8000', (Join-Path $ici 'partition-table.bin'),
          '0x10000', (Join-Path $ici 'glycemie_display.bin'))
& $esptool[0] @($esptool[1..($esptool.Count-1)] + $args)
if ($LASTEXITCODE -ne 0) { Write-Host "L'installation a echoue." -ForegroundColor Red; exit 1 }
Write-Host ''
Write-Host "C'est installe. L'ecran redemarre et affiche l'assistant de configuration." -ForegroundColor Green
Write-Host "Choisissez la langue, le reseau Wi-Fi, puis saisissez votre compte Dexcom."
'@ | Set-Content -Path (Join-Path $livraison 'installer.ps1') -Encoding UTF8

# --- 3 bis. Imprint SOURCES qui ont produit ce programme ------------------------------------
# It later verifies that the book program comes from these sources, whatever
# be the dates of the files (renewing the public copy makes them all "recent").
# Le chemin du projet contient un accent : on le passe en ARGUMENT (one string, encoded)
# correctly) rather than in the code of a python -c, or it would be damaged.
$empreinte = & python (Join-Path $racine 'tools\preparer_paquet.py') --empreinte
if ($LASTEXITCODE -ne 0 -or -not $empreinte) { throw "Empreinte des sources impossible a calculer." }
"$empreinte  sources du firmware (main, components, config, CMakeLists, sdkconfig.defaults, partitions.csv)" |
    Set-Content -Path (Join-Path $livraison 'sources.sha256') -Encoding ascii

# --- 4. Empreintes --------------------------------------------------------------------------------
$lignes = Get-ChildItem $livraison -File | Where-Object { $_.Name -ne 'SHA256SUMS.txt' } |
    Sort-Object Name | ForEach-Object {
        '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name
    }
$lignes | Set-Content -Path (Join-Path $livraison 'SHA256SUMS.txt') -Encoding ascii

Write-Host ''
Write-Host "Fichiers prets a publier : $livraison" -ForegroundColor Green
Get-ChildItem $livraison -File | Sort-Object Name | ForEach-Object {
    '{0,-28} {1,8:N2} Mo' -f $_.Name, ($_.Length / 1MB)
}
Write-Host ''
Write-Host "Verifiez maintenant qu'aucun identifiant ne s'y trouve :" -ForegroundColor Cyan
Write-Host "    python tools\controle_donnees_perso.py --binaire livraison\glycemie_display.bin --binaire livraison\GlycemieConnexion.exe"
