/*

  softosc - software-based oscillators
  Tell a pin to flip a number of times with a given interval.
  higher level: tell a pin to oscillate with a given frequency for
   a given duration.
  any two concurrent oscillations need a "channel". Every channel can be
  configured for a pin (0-7 is Port B, 8-15 is Port C and 16-23 is Port D)
  and may be re-configured for a different pin if it is not currently in use.
  Every running channel uses up a slot in events.h's event queue.

  Copyright (c) 2019 Thomas Kremer

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#ifndef __SOFTOSC_H__
#define __SOFTOSC_H__

#include "events.h"

#ifndef SOFTOSC_CHANNELS
#define SOFTOSC_CHANNELS 1
#endif

#define PORT(k) (*(&PORTB + 3*(k)))
#define PIN(k) (*(&PINB + 3*(k)))
#define DDR(k) (*(&DDRB + 3*(k)))

// TODO: substitute pin by (port,mask) for performance vs. memory?
// TODO: maybe change from array-of-channels model to pointer-to-context model?
struct {
  uint32_t delay, count;
  int8_t pin;
} softosc_channels[SOFTOSC_CHANNELS];

static void EVENT_softosc_done(uint8_t channel);

static void softosc_init() {
  // no-op for now. But maybe clear all channel data.
  for (int i = 0; i < SOFTOSC_CHANNELS; i++)
    softosc_channels[i].count = 0;
}

// must not be running yet.
static void softosc_set_pin(int16_t channel, int8_t pin) {
  softosc_channels[channel].pin = pin;
  softosc_channels[channel].count = 0;
  softosc_channels[channel].delay = 0;
}

static void softosc_event(void* param) {
  uint32_t chan = (int16_t) param;
  int8_t pin = softosc_channels[chan].pin;
  PIN(pin>>3) = 1 << (pin & 7);

  //int16_t i = (int16_t)param;
  uint32_t c = softosc_channels[chan].count;

  if (c > 0) {
    if (c != 0xffffffff) {
      c--;
      softosc_channels[chan].count = c;
    }
    enqueue_event_rel(softosc_channels[chan].delay,&softosc_event,param);
  } else {
    DDR(pin>>3) &= ~(1 << (pin & 7));
    PORT(pin>>3) |= 1 << (pin & 7);
    EVENT_softosc_done(chan);
  }
}

// takes pin-toggle delay and count of pin toggles.
static void hw_softosc(uint8_t chan, uint32_t delay, uint32_t count) {
  softosc_channels[chan].delay = delay;
  softosc_channels[chan].count = count;
  int8_t pin = softosc_channels[chan].pin;
  PORT(pin>>3) &= ~(1 << (pin & 7));
  DDR(pin>>3) |= 1 << (pin & 7);
  enqueue_event_rel(1,&softosc_event,(void*)(int16_t)chan); //(void*)count);
}

static void hw_softosc_forever(uint8_t chan, uint32_t delay) {
  hw_softosc(chan,delay,0xffffffff);
}

// takes frequency in Hz and duration in msec.
static void softosc(uint8_t chan, uint16_t freq, uint32_t msec) {
  uint32_t delay = ((uint32_t)sec2ticks(1,TIMER_DIV))/(2*freq);
  uint32_t count = ((uint32_t)freq*2*msec)/1000; // freq == 1/2 sec
  hw_softosc(chan,delay,count);
}

static void softosc_forever(uint8_t chan, uint16_t freq) {
  uint32_t delay = ((uint32_t)sec2ticks(1,TIMER_DIV))/(2*freq);
  hw_softosc(chan,delay,0xffffffff);
}

// does not immediately stop. Just marks the next flip as the last.
static void softosc_stop(uint8_t chan) {
  softosc_channels[chan].count = 0;
  //softosc_channels[chan].delay = 0;
}

// does not immediately stop. Just marks the next flip as the last.
static void softosc_stop_all() {
  softosc_init(); // just the same thing.
  /*
  for (int i = 0; i < SOFTOSC_CHANNELS; i++)
    softosc_channels[i].count = 0;
  */
}

// stops immediately, but does not tidy up.
static void softosc_break_all() {
  dequeue_events(&softosc_event);
}
#define SOFTOSC_FREQ_TO_DELAY(freq) ((uint32_t)sec2ticks(1.0/(2*(freq)),TIMER_DIV))
#define SOFTOSC_MS_TO_COUNT(freq,msec) (((uint32_t)(freq)*2*(msec))/1000);

#ifdef BEEP_PIN

#ifndef BEEP_CHANNEL
#define BEEP_CHANNEL 0
#endif

// must not be running yet.
static void beep_init() {
  softosc_set_pin(BEEP_CHANNEL, BEEP_PIN);
}

// takes pin-toggle delay and count of pin toggles.
static void hw_beep(uint32_t delay, uint32_t count) {
  hw_softosc(BEEP_CHANNEL, delay, count);
}

static void hw_beep_forever(uint32_t delay) {
  hw_softosc_forever(BEEP_CHANNEL,delay);
}

// takes frequency in Hz and duration in msec.
static void beep(uint16_t freq, uint32_t msec) {
  softosc(BEEP_CHANNEL, freq, msec);
}

static void beep_forever(uint16_t freq) {
  softosc_forever(BEEP_CHANNEL, freq);
}

// does not immediately stop. Just marks the next flip as the last.
static void beep_stop() {
  softosc_stop(BEEP_CHANNEL);
}

#endif

#endif
