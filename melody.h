/*

  melody.h - define melodies located in progmem and play them via beep()

  Copyright (c) 2019 Thomas Kremer

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#ifndef __MELODY_H__
#define __MELODY_H__

#include <avr/pgmspace.h>
#include "timers.h"
#include "beep.h"
#include "math.h"

#define MELODY_NOTE_c 60
#define MELODY_NOTE_cis 61
#define MELODY_NOTE_d 62
#define MELODY_NOTE_dis 63
#define MELODY_NOTE_e 64
#define MELODY_NOTE_eis 65
#define MELODY_NOTE_f 65
#define MELODY_NOTE_fis 66
#define MELODY_NOTE_g 67
#define MELODY_NOTE_gis 68
#define MELODY_NOTE_a 69
#define MELODY_NOTE_ais 70
#define MELODY_NOTE_b 71
#define MELODY_NOTE_bis 72

#define MELODY_NOTE_ces 59
#define MELODY_NOTE_des 61
#define MELODY_NOTE_es  63
#define MELODY_NOTE_fes 64
#define MELODY_NOTE_ges 66
#define MELODY_NOTE_as  68
#define MELODY_NOTE_bes 70

// a break is a long delay with as few clicks as possible in between.
// actually we'd want a way to specify a 32-bit delay, but that would waste
// a lot of memory.
#define melody_entry(pitch_,len_) { .pitch=(pitch_), .len=(len_) }
#define melody_rest(quant) melody_entry(0xff,quant),
#define melody_stopmarker() melody_entry(0,0),

//#define melody_note(num,msec) melody_entry(440*pow(2,(num-9)/12),msec),

#define melody_bpm_unit(bpm) (sec2ticks(60,TIMER_DIV)/(bpm))

// pow should really be allowed for compile-time constants...
// well, let's just make a compile-time lookup table for 2^(k/12):
// perl -e 'for (0..11) { printf "  (k) == %2d ? %.30f : \\\n", $_, 2**($_/12); }'
#define pow2_12_k(k,v,type) ( \
  (k) ==  0 ? type (1.000000000000000000000000000000*(v)) : \
  (k) ==  1 ? type (1.059463094359295309843105314940*(v)) : \
  (k) ==  2 ? type (1.122462048309373017218604218215*(v)) : \
  (k) ==  3 ? type (1.189207115002721026897347655904*(v)) : \
  (k) ==  4 ? type (1.259921049894873190666544360283*(v)) : \
  (k) ==  5 ? type (1.334839854170034367797370578046*(v)) : \
  (k) ==  6 ? type (1.414213562373095145474621858739*(v)) : \
  (k) ==  7 ? type (1.498307076876681520616330089979*(v)) : \
  (k) ==  8 ? type (1.587401051968199361397182656219*(v)) : \
  (k) ==  9 ? type (1.681792830507429004072150746651*(v)) : \
  (k) == 10 ? type (1.781797436280678548214950751571*(v)) : \
  (k) == 11 ? type (1.887748625363386834052903395786*(v)) : \
          type (2.0*(v)))
// this should only be called with constant n:
#define pow2_12(n) (pow2_12_k(((n)%12+12)%12,1,) \
                   * ((n) >= 0 ? (1 << ((n)/12)) : 1.0/(1 << (-(n)/12))))

/*
#define proper_shift(x,n) ((x) \
                        >> ((n) >= 0 ? 0 : -(n)) \
                        << ((n) <= 0 ? 0 : (n)))

#define proper_div(a,b) ((a) >= 0 ? (a)/(b) : (a-b+1)/(b))
*/

// this should be called with n a variable and v a constant:
#define pow2_12v(n,v) (pow2_12_k(((n)%12+12)%12,(v),(uint32_t)) \
                        >> ((n) >= 0 ? 0 : -(n-12+1)/12) \
                        << ((n) <= 0 ? 0 : (n)/12))
// FIXME: probably something wrong if n < 0

#define melody_delay440 BEEP_FREQ_TO_DELAY(440)

// a4 = 69; c = 60
//#define melody_midi(num,msec) melody_entry(440*pow(2,((num)-69)/12),msec),
#define melody_midi(num,len) melody_entry(num,len),
#define melody_note(note,oct,quant) melody_midi(MELODY_NOTE_##note + (oct)*12,(quant))
//#define melody_note(note,oct,quant,unit) melody_midi(MELODY_NOTE_##note + (oct)*12,(quant))

//// hard-coded little endianness here...
//#define dword2bytepairs(d) {(d)&0xff,((d)>>8)&0xff},{((d)>>16)&0xff,((d)>>24)&0xff}
//// We have to do a hack here. Actually we want a struct with a header uint32_t and a variable-sized array after that. But it is an imperfect world and we don't get everything we want, so we just make an array of notes where the first two just happen to encode our header...
////  dword2bytepairs(melody_bpm_unit(bpm)), 

#define melody_begin(name,bpm) \
  const melody_t name PROGMEM = {\
  .time_unit=melody_bpm_unit(bpm), .notes = {

#define melody_end() \
  { .pitch=0, .len=0 } }\
};

typedef const struct {
  uint8_t pitch, len;
} note_t;

typedef const struct {
  uint32_t time_unit;
  note_t notes[];
} melody_t;

/*
const melody_t melody PROGMEM = {
  .time_unit=1234, .notes = {
    melody_note(a,0,2,100)
    melody_note(b,0,3,100)
    melody_note(c,0,1,100)
    { .pitch=0, .len=0 }
  }
};
*/

note_t* melody_next = NULL; // pointer to PROGMEM
//uint32_t melody_time_unit = melody_bpm_unit(240);
uint32_t melody_time_unit = melody_bpm_unit(2*151);

void EVENT_melody_done();

// accessors
note_t* melody_get_notes(melody_t* melody) {
  return &melody->notes[0];
}

uint32_t melody_get_time_unit(melody_t* melody) {
  return pgm_read_dword(&melody->time_unit);
}

void melody_set_time_unit(uint32_t unit) {
  melody_time_unit = unit;
}

void melody_next_note() {
  uint32_t delay,count;

  if (!melody_next) {
    EVENT_melody_done();
    return;
  }
  uint8_t pitch = pgm_read_byte(&melody_next->pitch);
  uint8_t len = pgm_read_byte(&melody_next->len);
  //freq = 440*(2^((pitch-69)/12))
  if (len != 0) {
    uint32_t ticks = (uint32_t)melody_time_unit*len;
    if (pitch != 0xff) {
      int16_t n = 69-pitch;
    // TODO: turn pow2_12v into a *real* lookup-table, as it seems to be not optimized.
    //delay = pgm_read_dword(&melody_base_delays[(n%12+12)%12]);
      delay = pow2_12v(n,melody_delay440);
      count = ticks/delay;
    } else {
      // break
      delay = ticks;
      count = 1;
    }

    melody_next++;
    hw_beep(delay,count);
  } else {
    melody_next = NULL;
    EVENT_melody_done();
  }
}

void EVENT_beep_done() {
  melody_next_note();
}

// notes must be in PROGMEM
void melody_play_notes(note_t* notes) {
  melody_next = &notes[0];
  melody_next_note();
}

// use stored time_unit to play the notes.
// melody must be in PROGMEM
void melody_play(melody_t* melody) {
  melody_set_time_unit(pgm_read_dword(&melody->time_unit));
  melody_play_notes(&melody->notes[0]);
}

// give fixed time_unit to play in
void melody_play_by_unit(melody_t* melody,uint32_t time_unit) {
  melody_set_time_unit(time_unit);
  melody_play_notes(&melody->notes[0]);
}

// give 8.8 fixed-point speed factor to play it.
void melody_play_by_factor(melody_t* melody,uint16_t factor) {
  // factor is 8.8 fixed-point (1.0 = 0x100)
  uint32_t time_unit = pgm_read_dword(&melody->time_unit);
  time_unit = (time_unit >> 8)*factor + (((time_unit&0xff)*factor) >> 8);
  /*
  // we need time_unit to fit in 24 bit here!
  time_unit *= factor;
  time_unit >>= 8;*/
  melody_play_by_unit(melody,time_unit);
}

void melody_stop() {
  melody_next = NULL;
  beep_stop();
}

#endif
