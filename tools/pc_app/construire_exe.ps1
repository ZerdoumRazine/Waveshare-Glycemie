# Builds the distributable Windows setup application.
#
# Design note: Always test the compiled executable before sharing it.
# Package the tested Windows USB assistant into a standalone executable.
#
<#
.SYNOPSIS
    Build GlycemieConnexion.exe, the Windows USB setup application.

.DESCRIPTION
    PyInstaller bundles Python, Tkinter and tools/assistant_config.py into a
    standalone EXE. End users do not need to install Python or other libraries.
    Run this builder after changing the Windows interface or serial assistant.

    Development prerequisite:
        py -3 -m pip install pyinstaller

.NOTES
    The executable is not code-signed. Windows SmartScreen may display
    an unknown publisher warning. See the English installation guide.
    The utility exchanges device configuration only over a local USB cable.
#>
param(
    [switch]$Propre   # Clean previous build and dist directories.
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

# Prefer the Windows py launcher; fall back to python on PATH.
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
# assistant_config is imported dynamically; force PyInstaller to include its
# Python dependencies instead of treating it as a plain data file.
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
