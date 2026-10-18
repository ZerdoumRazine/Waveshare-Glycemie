/*
 * Reception de la banque vocale par le cable USB-serie de la console (outil temporaire, PC :
 * tools/voice/sd_voice_upload.py) : copie dans /glycemie/audio/voice/ sans retirer la microSD ni utiliser le
 * reseau. Integrite : CRC32 de chaque fichier recu en memoire, puis relu sur la carte apres ecriture.
 *
 *   vrx_begin [debit]              session, debit temporaire 115200 a 921600 (VRX BEGIN OK ...)
 *   vrx_ping                       -> VRX PONG
 *   vrx_put <nom> <taille> <crc32> -> VRX SKIP (fichier identique deja present) | VRX READY, puis blocs binaires
 *                                     de 896 octets au plus, chacun acquitte (VRX ACK <octets recus>)
 *                                     -> VRX OK <nom> <taille> <crc32> | VRX ERR <raison>
 *   vrx_dir <code> [lecture]       dossier de langue des commandes suivantes (/glycemie/audio/voice/<code>) :
 *                                     cree au besoin, sauf en mode "lecture" ou il doit exister (aucune ecriture) ;
 *                                     "vrx_dir -" revient a la racine (banque francaise) -> VRX DIR ...
 *   vrx_list [crc]                 -> VRX FILE <nom> <taille> [crc32] ... puis
 *                                     VRX LIST dossier= fichiers= octets= libre_mo= ; le dossier est celui choisi
 *                                     par vrx_dir : verification en LECTURE SEULE de n'importe quelle langue
 *   vrx_inject <taille> <crc32>    (test K19) PCM 16 bits mono 16 kHz recu comme vrx_put puis ajoute au signal des
 *                                     micros de l'ecoute vocale, sans rien ecrire sur la carte -> VRX OK inject ...
 *   vrx_end                        -> VRX END, retour a 115200 (automatique apres 30 s sans commande)
 */
#pragma once

#include <stdbool.h>

/* Console : true si la ligne est une commande vrx (traitee) */
bool voice_rx_command(const char *line);

/* Console, appel periodique : fin d'une session inactive et retour au debit normal */
void voice_rx_idle(void);
