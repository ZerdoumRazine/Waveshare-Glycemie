# Builds the distributable Windows setup application.
#
# Design note: Always test the compiled executable before sharing it.
# Package the tested Windows USB assistant into a standalone executable.
#
<#
.SYNOPSIS
    Manufactures the Windows application "WaveShare Glycemie — connection" (one .exe file).

.DESCRIPTION
    The result, dist\GlycemieLogin.exe, opens by double-click: the end user has
    NI need of NI Python d'aucune bibliotheque. L'Executable embarks the interpretation, tkinter and
    tools\assistant_config.py (the same dialog as the command line assistant).

    To execute once after each modification of the application or assistant:
        powershell -ExecutionPolicy Bypass -File tools\pc_app\construire_exe.ps1

    Prerequisites on the development PC only: Python 3 and PyInstall
        py -3 -m pip install pyinstaller

.NOTES
    The executable is PAS signe : Windows peut afficher « Editeur inconnu » au premier lancement
    (SmartScreen). This is explained in the tutorial. Nothing is sent over the Internet by the app.
#>
param(
    [switch]$Propre   # repart de zero (supprime build\ et dist\)
)
$ErrorActionPreference = 'Stop'

$ici      = Split-Path -Parent $MyInvocation.MyCommand.Path
$racine   = Resolve-Path (Join-Path $ici '..\..')
$script   = Join-Path $ici 'glycemie_connexion.pyw'
$assistant = Join-Path $racine 'tools\assistant_config.py'
$sortie   = Join-Path $ici 'dist'
$travail  = Join-Path $ici 'build'

foreach ($f in @($script, $assistant)) {
    if (-not (Test-Path $f)) { throw "Fichier introuvable : $f" }
}

# Python : py -3 si disponible, sinon python du PATH
$python = 'python'
if (Get-Command py -ErrorAction SilentlyContinue) { $python = 'py' }
$argsPy = @()
if ($python -eq 'py') { $argsPy += '-3' }

& $python @argsPy -m PyInstaller --version *>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller n'est pas installe. Lancez :  $python $($argsPy -join ' ') -m pip install pyinstaller"
}

if ($Propre) {
    foreach ($d in @($sortie, $travail)) { if (Test-Path $d) { Remove-Item -Recurse -Force $d } }
}

Write-Host "Construction de l'application Windows..." -ForegroundColor Cyan
# --noconsole: no black windows; --onefile: one .exe to give to the user
# --paths + --hidden-import: assistant_config.py is loaded like MODULE, so that PyInstiller
# analyse aussi ses propres imports (getpass, re, argparse, winreg...). Embarque en simple donnee,
# these modules would be missing and'application ne s'Would not open.
& $python @argsPy -m PyInstaller `
    --noconfirm --clean --onefile --noconsole `
    --name GlycemieConnexion `
    --distpath $sortie --workpath $travail --specpath $travail `
    --paths (Join-Path $racine 'tools') `
    --hidden-import assistant_config `
    $script
if ($LASTEXITCODE -ne 0) { throw "La construction a echoue (code $LASTEXITCODE)." }

$exe = Join-Path $sortie 'GlycemieConnexion.exe'
if (-not (Test-Path $exe)) { throw "L'executable attendu est absent : $exe" }
$h = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLower()
$taille = [math]::Round((Get-Item $exe).Length / 1MB, 2)
Write-Host ""
Write-Host "Application prete : $exe" -ForegroundColor Green
Write-Host ("Taille : {0} Mo" -f $taille)
Write-Host "SHA-256 : $h"
$h | Set-Content -Path (Join-Path $sortie 'GlycemieConnexion.exe.sha256') -Encoding ascii
