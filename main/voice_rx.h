/* voice_rx.h: USB serial voice-file transfer protocol.
 * Shared declarations for the implementation.
 */
/*
 * Reception of voice bank by cable USB- console series (temporary tool, PC:
 * tools/voice/sd_voice_upload.py) : copy in /glycemie/audio/voice/ without removing the microSD or using the
 * reseau. Integrite : CRC32 of each file received in memory, then reread on the card after writing.
 *
 *   vrx_begin [debit]              session, debit temporaire 115200 a 921600 (VRX BEGIN OK ...)
 *   vrx_ping                       -> VRX PONG
 *   vrx_put <nom> <taille> <crc32> -> VRX SKIP (Identical file already present) VRX READY, puis blocs binaires
 *                                     de 896 octets au plus, chacun acquitte (VRX ACK <octets recus>)
 *                                     -> VRX OK <nom> <taille> <crc32> | VRX ERR <raison>
 *   vrx_dir <code> [read] language folder of the following commands (/glycemie/audio/voice/<code>) :
 *                                     created as needed, except in "read" mode or it must exist (no writing);
 *                                     "vrx_dir -" returns to the root (French bank) -> VRX DIR ...
 *   vrx_list [crc]                 -> VRX FILE <nom> <taille> [crc32] ... puis
 *                                     VRX LIST folder= files= bytes= libre_mo= the folder is the one chosen
 *                                     par vrx_dir : verification en LECTURE SEULE de n'importe quelle langue
 *   vrx_inject <size> <crc32> (test K19) PCM 16 bits mono 16 kHz received as vrx_put then add to the signal
 *                                     Voice listening microphones, without writing anything on the map -> VRX OK inject ...
 *   vrx_end                        -> VRX END, back to 115200 (automatic after 30 s without order)
 */
/* USB serial commands for adding and verifying microSD voice clips.
 */
#pragma once

#include <stdbool.h>

/* Console: true if the line is a vrx command (treaty) */
bool voice_rx_command(const char *line);

/* Console, periodic call: end of inactive session and return to normal flow */
void voice_rx_idle(void);
