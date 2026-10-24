/* voice_rx.h: USB serial voice-file transfer protocol.
 * Shared declarations for the implementation.
 */
/* UART/CH343 protocol for installing or checking optional WAV speech banks
 * while the microSD remains in the display. The PC uses
 * tools/voice/sd_voice_upload.py. Each received file is verified by CRC32
 * and read back after writing; the tool never formats the card.
 *
 * Commands:
 *   vrx_begin [baud]             Start a temporary transfer session.
 *   vrx_ping                     Check connectivity (VRX PONG).
 *   vrx_put <name> <size> <crc>  Send a file in acknowledged binary chunks.
 *   vrx_dir <lang> [read]        Select a language folder; read forbids creation.
 *   vrx_list [crc]               Enumerate clips and optional CRCs, without writing.
 *   vrx_inject <size> <crc>      Inject PCM test audio, without microSD writes.
 *   vrx_end                      End the session and restore the serial baud rate.
 * French recordings use the default voice directory; other languages use
 * their language-code subfolders. */
/* Serial entry points for installing and verifying recorded speech clips. */
#pragma once

#include <stdbool.h>

/* Return true when a command belongs to the VRX transfer protocol. */
bool voice_rx_command(const char *line);

/* End an idle transfer session and restore normal serial settings. */
void voice_rx_idle(void);
