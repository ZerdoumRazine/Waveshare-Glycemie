# Runs local speech generation on Windows.
#
# Design note: Generated audio may have separate redistribution conditions.
# Generate legacy Windows speech samples for private development, not public distribution.
#
# Generate French vocal extracts (WAV 16 kHz, 16 bits, mono) with Windows voice en-FR installs.
# No Internet dependence, no installation. Output: tools\voice\wav\<id>.wav
# Usage : powershell -ExecutionPolicy Bypass -File tools\voice\gen_voice.ps1

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Speech

$outDir = Join-Path $PSScriptRoot 'wav'
New-Item -ItemType Directory -Force $outDir | Out-Null

# identifier -> text pronounce
$clips = [ordered]@{
    'p_tres_basse'    = 'Attention, glycémie très basse.'
    'p_basse'         = 'Attention, glycémie basse.'
    'p_elevee'        = 'Glycémie élevée.'
    'p_tres_elevee'   = 'Attention, glycémie très élevée.'
    'p_aucune_donnee' = 'Aucune nouvelle donnée depuis'
    'p_minutes'       = 'minutes.'
    'p_capteur_48h'   = 'Le capteur se termine dans deux jours.'
    'p_capteur_24h'   = 'Le capteur doit être remplacé demain.'
    'p_capteur_12h'   = 'Le capteur se termine dans douze heures.'
    'p_capteur_grace' = 'Le capteur est en période de grâce.'
    'p_capteur_2h'    = 'Le capteur doit être remplacé dans deux heures.'
    'p_test'          = 'Test du volume.'
    'p_glycemie'      = 'Glycémie'
    'p_mg_dl'         = 'milligrammes par décilitre.'
    'w_un'            = 'un'
    'w_deux'          = 'deux'
    'w_trois'         = 'trois'
    'w_quatre'        = 'quatre'
    'w_cinq'          = 'cinq'
    'w_six'           = 'six'
    'w_sept'          = 'sept'
    'w_huit'          = 'huit'
    'w_neuf'          = 'neuf'
    'w_dix'           = 'dix'
    'w_onze'          = 'onze'
    'w_douze'         = 'douze'
    'w_treize'        = 'treize'
    'w_quatorze'      = 'quatorze'
    'w_quinze'        = 'quinze'
    'w_seize'         = 'seize'
    'w_vingt'         = 'vingt'
    'w_trente'        = 'trente'
    'w_quarante'      = 'quarante'
    'w_cinquante'     = 'cinquante'
    'w_soixante'      = 'soixante'
    'w_quatre_vingt'  = 'quatre-vingt'
    'w_quatre_vingts' = 'quatre-vingts'
    'w_cent'          = 'cent'
    'w_cents'         = 'cents'
    'w_et'            = 'et'
}

$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
$voice = $synth.GetInstalledVoices() | Where-Object { $_.VoiceInfo.Culture.Name -eq 'fr-FR' -and $_.Enabled } | Select-Object -First 1
if (-not $voice) { throw 'Aucune voix fr-FR installee' }
$synth.SelectVoice($voice.VoiceInfo.Name)
$synth.Rate = 0
$format = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(16000, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)

foreach ($id in $clips.Keys) {
    $path = Join-Path $outDir ($id + '.wav')
    $synth.SetOutputToWaveFile($path, $format)
    $synth.Speak($clips[$id])
    $synth.SetOutputToNull()
}
$synth.Dispose()
"Voix : " + $voice.VoiceInfo.Name
"{0} extraits generes dans {1}" -f $clips.Count, $outDir
