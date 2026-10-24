# Everyday use of WaveShare Glycémie

This guide describes the five touchscreen pages and daily controls of the experimental family glucose display. It does **not** replace the official Dexcom app, receiver or alarms.

## The top status bar

The same header appears across pages. It summarizes important connectivity, clock and display information. Always check whether the glucose value is current; a large number on screen does not mean that Share delivered a recent reading.

## Home

The Home page emphasizes the latest glucose value (mg/dL), trend arrow, change since the preceding reading and age of the data. It is designed to be readable at a distance. Missing Wi-Fi, stale data or an uncertain trend should remain clearly visible.

## Graph

History can be viewed over several time windows (typically 3, 6, 12 or 24 hours). Touch exploration lets the user inspect readings. Gaps reflect missing samples: the display must not invent a measured value in those intervals.

## Today

The Today page summarizes the current day from midnight, including available readings, time in range, average, minimum, maximum, low/high periods and data coverage. Partial coverage reduces the usefulness of the summary; it should not be mistaken for a full day's record.

## Consumables

The G7 sensor page tracks insertion date, its 10-day scheduled end, and reminders near replacement. The infusion-set section tracks the selected 48- or 72-hour interval and replacement reminders. These are convenience reminders, **not medical prescriptions**.

Use the touchscreen confirmation flow when recording a new sensor or infusion set. Local stock quantities are reduced once for a confirmed replacement, and dates are saved persistently across normal restarts.

## Settings and connection

Settings includes volume, spoken announcements, alert options and thresholds, brightness, night display, language, stock-related options and the **Connection** menu. Connection opens the Wi-Fi/Dexcom wizard to test replacement credentials before saving them. The Windows USB app can perform the same setup without compiling firmware.

**Do not clear a working account just to test the wizard.** If the screen reports an error, keep the existing connection and use the troubleshooting instructions.

## Alerts

Alerts can be shown on screen and accompanied by beeps or prerecorded speech. The system distinguishes different glucose bands and missing-data conditions. Acknowledgment and snooze actions affect this supplementary display, not the official Dexcom safety alarm channel.

## Night mode

The night view reduces brightness on the selected schedule and allows a temporary return to normal display by touch. Important warnings retain their priority. Adjust brightness to the room rather than assuming maximum output improves reliability.

## Display and speech languages

The touchscreen and the optional WAV announcements support French, English, Spanish, German, Italian and Brazilian Portuguese. Select the language under Settings. The microSD must contain that language's bank for spoken numbers and phrases; French WAV files are at `glycemie/audio/voice/`, while other languages use subfolders.

Voice-**command recognition** is disabled in the standard public build. The touch interface remains the supported way to control the device.

## Demo mode

Demo mode shows clearly labeled fictitious readings so screens and graphics can be explored without a Dexcom account. Never confuse its demonstration values with real patient data.

## Daily checklist

Keep the official phone application and alarms active, watch for stale readings or network interruptions, retain microSD backups if using history, and keep backup/diagnostic files private.

For setup and recovery, see [Beginner's tutorial](BEGINNER_TUTORIAL_EN.md), [Troubleshooting](TROUBLESHOOTING_EN.md) and [Known limitations](LIMITATIONS_EN.md).
