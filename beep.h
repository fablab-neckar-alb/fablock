/*

  beep.h - play rectangular beeps with a speaker attached to BEEP_PORT/BEEP_PIN

  Copyright (c) 2019 Thomas Kremer

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#ifndef __BEEP_H__
#define __BEEP_H__

#include "timers.h"
#include "events.h"

#ifndef BEEP_PIN
#define BEEP_PORT C
#define BEEP_PIN 5
#endif
// TODO: error if not defined BEEP_PORT
#define BEEP_CONCAT(x,y) x##y
#define BEEP_CONCAT2(x,y) BEEP_CONCAT(x,y)
#define BEEP_PORT_PIN  BEEP_CONCAT2(PIN,BEEP_PORT)
#define BEEP_PORT_PORT BEEP_CONCAT2(PORT,BEEP_PORT)
#define BEEP_PORT_DDR  BEEP_CONCAT2(DDR,BEEP_PORT)

uint32_t beep_delay;
uint32_t beep_count;

void EVENT_beep_done();

void beep_event(void* param) {
  BEEP_PORT_PIN = 1 << BEEP_PIN;

  //int16_t i = (int16_t)param;
  uint32_t i = beep_count;

  if (i > 0) {
    i--;
    beep_count = i;
    enqueue_event_rel(beep_delay,&beep_event,NULL);//(void*)i);
  } else {
    BEEP_PORT_DDR &= ~(1 << BEEP_PIN);
    BEEP_PORT_PORT |= 1 << BEEP_PIN;
    EVENT_beep_done();
  }
}

/*
void EVENT_beep_done() {
  // nothing for now.
}
*/

void beep_stop() {
  dequeue_events(&beep_event);
  beep_delay = 0;
  beep_count = 0;
  BEEP_PORT_DDR &= ~(1 << BEEP_PIN);
  BEEP_PORT_PORT |= 1 << BEEP_PIN;
  EVENT_beep_done();
}

// takes pin-toggle delay and count of pin toggles.
void hw_beep(uint32_t delay, uint32_t count) {
  beep_delay = delay;
  beep_count = count;
  //uint16_t count = count;
  BEEP_PORT_PORT &= ~(1 << BEEP_PIN);
  BEEP_PORT_DDR |= 1 << BEEP_PIN;
  dequeue_events(&beep_event);
  enqueue_event_rel(1,&beep_event,NULL); //(void*)count);
  // FIXME: is it really necessary to queue it rather than just calling it?
}

// takes frequency in Hz and duration in msec.
void beep(uint16_t freq, uint32_t msec) {
  uint32_t delay = ((uint32_t)sec2ticks(1,TIMER_DIV))/(2*freq);
  uint32_t count = ((uint32_t)freq*2*msec)/1000; // freq == 1/2 sec
  hw_beep(delay,count);
}
#define BEEP_FREQ_TO_DELAY(freq) ((uint32_t)sec2ticks(1.0/(2*(freq)),TIMER_DIV))
#define BEEP_MS_TO_COUNT(freq,msec) ((uint32_t)((freq)*1.0*2*(msec)/1000))

#endif
