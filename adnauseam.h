/*

  adnauseam - play a couple of melodies in an infinite loop

  Copyright (c) 2019 Thomas Kremer

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#ifndef __ADNAUSEAM_H__
#define __ADNAUSEAM_H__

#include <avr/pgmspace.h>
#include "melody.h"

/*
#ifdef MELODY_DEFINE_KOROBEINIKI
melody_t *const korobeiniki_progression[] PROGMEM = {
  &korobeiniki_a,
  &korobeiniki_a,
  &korobeiniki_b,
  NULL
};
#endif
*/

melody_t*const* adnauseam_prog = NULL;
uint8_t adnauseam_next = 0;

void adnauseam_next_melody() {
  if (adnauseam_prog) {
    void* mel = pgm_read_ptr(&adnauseam_prog[adnauseam_next]);
    if (!mel) {
      adnauseam_next = 0;
      mel = pgm_read_ptr(&adnauseam_prog[adnauseam_next]);
      if (!mel) {
        return;
      }
    }
    adnauseam_next++;
    melody_play(mel);
  }
}

void EVENT_melody_done() {
  adnauseam_next_melody();
}

// *prog must be in progmem and null-terminated. **prog must also be in progmem.
void adnauseam_play(melody_t* const* prog) {
  adnauseam_prog = prog;
  adnauseam_next = 0;
  adnauseam_next_melody();
}

void adnauseam_stop() {
  adnauseam_prog = NULL;
  melody_stop();
}

#endif
