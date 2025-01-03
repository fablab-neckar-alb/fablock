/*

  Custom pinpad driver. Consumes 1 pin on Port C.

  Copyright (c) 2024 Paul Rosset
  Note: Code was extracted from pinpad.h
  text: Ideally we want a linear distribution of the pinad values this is possible for a linear keypad

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#ifndef __PINPAD_LINEAR_H__
#define __PINPAD_LINEAR_H__

/*
  # Linear Keypad

  The linear pinpad is a numerical+"*"+"#" 12-button keyboard wired so
  that it can be read by one analog pin. This pin is also configured as
  a wakeup-interrupt for the "*" key and should provide a limited attack
  surface for anyone with access to all external parts.

  ## Requirements

  A pin with both ADC and interrupt capability (C0..C5).

  The pinpad hardware:
    12+1 pins, one for each button plus mass.

    Pinpad connection:
      leftmost pin (1) ----> rightmost pin (13)
      *, 7, 1, 4, 0, 8, 5, 2, #, 9, 6, 3, mass


  ## External Wiring


    K+ --+-----+-----+--...-----+
         B0    B1    B2 ...     Bn
    K- --+-R1'-+-R2'-+--...-Rn'-+ (connector)

  ## Resistor Values

  The resistor values have been chosen in this approach so that
  the resulting ADC values will form a linear sequence that can
  be used to get the pressed key by entering the adc value in a
  linear term. This is quite elegant and can save several bytes
  of memory. Also Ri are chosen so that roughly equal voltage
  drops appear between adjacent keys.

  This means we want to spread the values evenly between MC_adc('*')
  and Adc_max.

  ## Calulation of optimal values

  The Optimal values are calculated by using the formula for MC_adc(i) and enter the value it for the lowest R_k('*') wich is 0
      MC_adc('*') = Adc_Range * (Rd)/(Rud)
      In order to split up the remaining Adc range we need to divide it by remaining the number of steps.
      To get the remaining value for the adc values we subtract the formula MC_adc('*') from the adc max value
      M_range = Adc_Range - MC_adc('*') = Adc_Range - Adc_Range * (Rd)/(Rud)
      M_range = Adc_Range * (1 - Rd/Rud)
      M_range = Adc_Range * (Rud/Rud - Rd/Rud)
      M_range = Adc_Range * ((Rud-Rd)/Rud)
      To get each step we have to divide this by the number of remaining steps n(inflated to 12 to have a value for not pessed which is infinite) and multiply it by i
      M_value(i) = Adc_Range * ((Rud-Rd)/Rud) * i / 12
      To get the real value we need to add MC_adc('*') 

      MC_adc(i) = Adc_Range * (Ru)/(Rud) + Adc_Range * ((Rud-Rd)/Rud) * i / 12
      MC_adc(i) = Adc_Range * ((Ru)/(Rud) + ((Rud-Rd)/Rud) * i / 12)
      or as voltage
      V_adc(i) = VCC * ((Rd)/(Rud) + ((Rud-Rd)/Rud) * i / 12)
      which is the initial formula iff Ru = Rud - Rd
      V_adc(i) = VCC * ((Rd)/(Rud) + (Ru/Rud) * i / 12)

      To get the Resistance for each button we can insert

      Ri = R_ges - Rud
      with V_u = VCC - Vi
      and  I_ges = Ru / (Vcc - V_adc(i))
      Ri = Ru / (1 - V_adc(i)/VCC) - Rud
      
      Thomas got this down to 
      TODO: add calculated way
      Ri = Rud*((i-1)/(n+1-i)) 
      To get Ri' we must subtract Ri-1
      Ri' = Ri - R_{i-1}     


      With the Values of the actual Resistor Network:
      Ru = 10k, 
      Rd = 1k 
      Rud := Ru+Rd, Rud=11k

      And the perl transcriptions
      Vi = perl -e 'print 93+int((1024-93)/12*$_),", " for 0..11; print "\n";'
      Ri = perl -e 'print int(11000*($_-1)/(12+1-$_)),", " for 1..12; print "\n";'
      
      Values
        Ri = (0, 1000, 2200, 3666, 5500, 7857, 11000, 15400, 22000, 33000, 55000, 121000)
        MC_adc(i)  = (93, 170, 248, 325, 403, 480, 558, 636, 713, 791, 868, 946)

      real by design:
        Ri' = (0, 1k, 1.2k, 1.5k, 1.8k, 4.7k/2, 3.3k, 4.7k, 6.8k, 10k, 22k, 68k)
        Ri = (0, 1k, 2.2k, 3.7k, 5.5k, 7.85k, 11.15k, 15.85k, 22.65k, 32.65k, 54.65k, 122.65k)
        V_i = VCC*(1-Ru/(Rud+Ri))
            = (0.45V, 0.83V, 1.21V, 1.60V, 1.97V, 2.35V, 2.74V, 3.14V, 3.51V, 3.85V, 4.24V, 4.63V)
            = VCC/1024*(93, 171, 248, 327, 403, 481, 562, 643, 720, 789, 868, 947)
*/


#include <avr/pgmspace.h>


// FIXME: replace ideal values by proper ranges.
//    OR: replace values by linear approximation, saving memory.
const int16_t pinpad_adc_values[12] PROGMEM =
    {93, 171, 248, 327, 403, 481, 562, 643, 720, 789, 868, 947};
    
const char pinpad_chars[12] PROGMEM =
    {'*', '7', '1', '4', '0', '8', '5', '2', '#', '9', '6', '3'};

// const int16_t pinpad_fuzz = 93/3;
//  idea: voltage space between k and k+1 is 1/3 for k, 1/3 invalid
//  and 1/3 for k+1. However it may as well be [x,1-2x,x] for any x<1/2.
// Remark by Paul min of k and k+1 is 69
#define pinpad_fuzz (23)
#define pinpad_max_valid (pinpad_adc_values[11] + pinpad_fuzz)
#define pinpad_min_idle (1023 - pinpad_fuzz)

/**
 * @brief Search for the corresponding pinpad key with a given adc value 
 * @param minval The minvalue of the adc pin.
 * @param return The key that was pressed or '\0' on error.
 */
inline char get_pinpad_key(int16_t minval){
  char pressedKey = 0;
  int16_t i = ((minval-93)*24/(1024-93)+1)/2;
  if (i < 0) i = 0;
  if (i > 11) i = 11;

  int16_t diff = abs(minval-(int16_t)pgm_read_word(&pinpad_adc_values[i]));
  if (diff < pinpad_fuzz) {
    pressedKey = pgm_read_byte(&pinpad_chars[i]);
  }
  return pressedKey;
}

#endif // __PINPAD_LINEAR_H__



