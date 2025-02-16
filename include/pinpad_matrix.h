/*

  Custom pinpad driver. Consumes 1 pin on Port C.

  Copyright (c) 2024 Paul Rosset

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#ifndef __PINPAD_MATRIX_H__
#define __PINPAD_MATRIX_H__


/*

  This implentation is for numerical pinpads with 12-button keyboard
  that are connected as a matrix. This means that there is one wire
  per row and a pin per line, 7 in total. 
  The pinpad keyboard wired so that it can be read by one analog pin, used as wakeup-interrupt at least for
  the "*" key.
  This is archieved by connecting each row and each column  
  resistor to the next line as displayed in schematic S1 below.


  # S1: Schematic for out matrix Keyboard adaption:

   1------2------3---+
   |      |      |   1K
   4------5------6---+
   |      |      |   1K
   7------8------9---+
   |      |      |   1K
   *------0------#---+--------- K+
   |      |      |
   +-4.7K-+-4.7K-+
   |
   +--------------------------- K-




  required: a pin with both ADC and interrupt capability (C0..C5).


  The Voltages where calulated using the formula MC_adc(i) of schematic S1 from pinpad.h

The resulting adc values for each key are displayed in the following table  

Key   R(i)   MC_adc(i)
'*'   0      93
'7'   1000   171
'4'   2000   236
'1'   3000   293
'8'   4700   372
'5'   5700   411
'2'   6700   445
'#'   7700   476
'9'   9400   522
'6'   10400  545
'6'   11400  567
'3'   12400  586


*/

#include <avr/pgmspace.h>
#include <stdlib.h>

const int16_t pinpad_adc_values[12] PROGMEM =
    {93, 171, 236, 293, 372, 411, 445, 476, 522, 545, 567, 586};
const char pinpad_chars[12] PROGMEM =
    {'*', '7', '4', '1', '0', '8', '5', '2', '#', '9', '6', '3'};


#define pinpad_min_dist (20)
#define pinpad_max_valid ((pinpad_adc_values[11]) + pinpad_min_dist)
#define pinpad_min_idle (1023 - pinpad_min_dist)
/*#if (pinpad_max_valid) >= pinpad_min_idle
  #error "Values of pinpad ranged overlap"
#endif*/



/**
 * @brief Search for the corresponding pinpad key with a given adc value 
 * @param value The minvalue of the adc pin.
 * @param return The key that was pressed or '\0' on error.
 */
inline char get_pinpad_key(int16_t value){
  unsigned char index = 0;
  uint16_t mindiff = 1024;
  // above measurement range no key was pressed
  if(value >= pinpad_max_valid || value < pinpad_min_dist){
    return '\0';
  }
  for (unsigned char i = 0; i < 12; i++)
  {
    int16_t diff = abs(value - (int16_t)pgm_read_word(&pinpad_adc_values [i]));
    if (diff < mindiff)
    {
      mindiff = diff;
      index = i;
    }
  }
  return (char)pgm_read_byte(&pinpad_chars[index]);
  
}

#endif // __PINPAD_MATRIX_H__