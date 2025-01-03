/*

  Custom pinpad driver. Consumes 1 pin on Port C.

  Copyright (c) 2019 Thomas Kremer

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#ifndef __PINPAD_H__
#define __PINPAD_H__

/*
  This is the common implementation to read a pressed key on the pinpad pin.
  

  Requirements: 
    * A pin with both ADC and interrupt capability (C0..C5).
    * Choose one of the surrported pinpad models by defining
      PINPAD_IMPLEMENTATION to LINEAR or MATRIX            

  internal wiring:
    We used C = 2*220nF, Dz = 12V Zener diode, soldered everything directly
    between two pin headers (everything in SMD parts except for Dz).

  # S1: Adc Conection to Arduino input pin:

    VCC -----+
             Ru
             +-------+--- *K+
             Rm      |
    MC_adc---+      ^Dz
             C       |
             +-------+--- *K-
             Rd
    GND-*----+

    Chosen Values:
      R_u = 10k 
      R_d = 1k
      R_m can be ignored as there is no current flowing over the ADC
      R_k(i) = chosen in pinpad_matrix.h or pinpad_linear.h
      The pinpad adc value at the MC pin can be calculated with the formula:
      MC_adc(i) = 1024 * (R_k(i) + R_u) / (R_k(i) + R_ud)

    Derived Values:
      The adc of our arduino has 10bit Resolution.
      Adc_range = 2^10 = 1024
      Adc_max = 2^10 - 1 = 1023
*/

// every implementation of a pipad needs the function get_pinpad_key and the value pinpad_min_idle
#ifdef PINPAD_LINEAR
#include "pinpad_linear.h"
#else
#ifdef PINPAD_MATRIX
#include "pinpad_matrix.h"
#else
#error "PINPAD_IMPLEMENTATION was not set"
#endif
#endif
#include <adc_watch.h>

/* This needs to be done elsewhere:
void EVENT_adc_watch(uint8_t channel, int16_t value) {
  if (channel == PINPAD_PIN)
    pinpad_on_adc_read(value);
}
void init() {
  PCICR |= 1<<PCIE1;
}
*/

// Port is always C
#ifndef PINPAD_PIN
#define PINPAD_PIN 0 // C0, ADC0
#endif // PINPAD_PIN


/// @brief Pinpad context to trac the currently monitored value
typedef struct
{
  int16_t minval;
} pinpad_ctx_t;
pinpad_ctx_t pinpad_ctx;

void EVENT_pinpad_keypressed(char c);
bool snprintl(char *s, int len, int32_t value);
/**
 * @brief pressing a key i means going down to a voltage that is within
 *        pinpad_adc_values[i], then going up again.
 * @param value todo
 */
void pinpad_on_adc_read(int16_t value)
{
  // Get the tracked value
  int16_t minval = pinpad_ctx.minval;
  // Was the value reset then start a new measurement with the current value
  if (minval == 1024)
  {
    // TODO:
    adc_watch_set_range(PINPAD_PIN, value - 5, value + 5);
  }
  // Update the tracked value if the value if it is lower
  if (value < minval)
  {
    minval = value;
    pinpad_ctx.minval = minval;
  }
  // Once the key has been released
  if (value > pinpad_min_idle)
  {
    char key = get_pinpad_key(minval);
    if(key != '\0'){
        EVENT_pinpad_keypressed(key);
    }
    // Reset traced value and adc watch range
    pinpad_ctx.minval = 1024;
    adc_watch_set_range(PINPAD_PIN, pinpad_max_valid, 1023);
  }
}

void pinpad_sleep()
{
  // need to activate digital input, disable reading
  uint8_t mask = (1 << PINPAD_PIN);
  PCMSK1 |= mask;
  DIDR0 &= ~mask;
  adc_watch_set_mask(adcw_state.mask & ~mask);
  // adc_watch_set_range(PINPAD_PIN,0,1023);
}

void pinpad_unsleep()
{
  // need to disable digital input, activate reading
  uint8_t mask = (1 << PINPAD_PIN);
  PCMSK1 &= ~mask;
  DIDR0 |= mask;

  adc_watch_set_range(PINPAD_PIN, pinpad_max_valid, 1023);
  adc_watch_set_mask(adcw_state.mask | mask);
}

void pinpad_init()
{
  pinpad_ctx.minval = 1024;
  DDRC &= ~(1 << PINPAD_PIN);
  PORTC &= ~(1 << PINPAD_PIN);
  // PORTC |= (1<<PINPAD_PIN);
  pinpad_unsleep();
}

void pinpad_stop()
{
  pinpad_sleep();
  pinpad_ctx.minval = 1024;
  PORTC |= (1 << PINPAD_PIN); // enable internal pull-up, just in case.
}

#endif
