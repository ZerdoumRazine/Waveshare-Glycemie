<#
.SYNOPSIS
    Prepare les fichiers a telecharger : programme PUBLIC de l'ecran + application Windows + empreintes.

.DESCRIPTION
    Le programme est compile depuis la COPIE PUBLIQUE (dossier Razine), qui ne contient aucun
    identifiant : ni reseau Wi-Fi, ni compte Dexcom. L'utilisateur saisira les siens apres
    l'installation, sur l'ecran tactile ou avec l'application Windows.

    Jamais le programme compile depuis le dossier de travail personnel : celui-la contient vos
    identifiants (private\device_secrets.h y est recopie pour votre propre appareil).

    Resultat dans livraison\ (hors depot) :
        glycemie_display.bin, bootloader.bin, partition-table.bin   le programme
        GlycemieConnexion.exe                                        l'application Windows
        installer.ps1                                                installation en une commande
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

# build.ps1 compile dans une copie de travail ASCII quand le chemin contient un accent
# Le @(...) exterieur impose un tableau : sans lui, un resultat unique serait une chaine,
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

# --- 3. Installation en une commande, pour la personne qui telecharge -----------------------------
@'
<#
    Installe le programme WaveShare Glycemie sur l'ecran, par le cable USB.

    Avant : branchez l'ecran au PC avec un cable USB-C de DONNEES, et fermez tout moniteur serie.
    Ensuite : clic droit sur ce fichier -> Executer avec PowerShell. Comptez deux minutes.

    Ce script n'efface pas la carte (pas d'erase_flash) : il remplace le programme, la table des
    partitions et l'amorce. Les reglages, les dates de consommables et la microSD sont conserves.
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

# --- 3 bis. Empreinte des SOURCES qui ont produit ce programme ------------------------------------
# Elle permet de verifier plus tard que le programme livre vient bien de ces sources-la, quelles que
# soient les dates des fichiers (regenerer la copie publique les rend toutes "recentes").
# Le chemin du projet contient un accent : on le passe en ARGUMENT (une seule chaine, encodee
# correctement) plutot que dans le code d'un python -c, ou il serait abime.
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
