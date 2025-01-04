#ifndef MOTOR_STEPPER_H
#define MOTOR_STEPPER_H
#include <stdint.h>
#include "timers.h"

// Note: current implementaiton only supports one IO-Port for enable and dir
#ifndef DOOR_MOTOR_PORT
#warning(Choosing default value for DOOR_MOTOR_PORT)
#define DOOR_MOTOR_PORT D
#endif

#ifndef MOTOR_DISABLE
#warning(Choosing default value for MOTOR_ENABLE)
#define MOTOR_DISABLE 2
#endif
#ifndef MOTOR_DIR
#warning(Choosing default value for MOTOR_DIR_PIN)
#define MOTOR_DIR 3
#endif


/*
  If we use a stepper with a stepper driver, we need a PWM pin.
    DOOR_MOTOR_PIN1 is the power enable pin
    DOOR_MOTOR_PIN2 is the direction pin
    TIMER0 is used for the PWM. (Timer 1 is used for events.h, Timer 2 is not yet implemented.)
    DOOR_MOTOR_OC must be 0 or 1 (OC0A/OC0B) (D6/D5) and is the step pin

  right now we don't really care about numbers of steps. Just use a frequency
  and a time, we have feedback anyway.
*/
#ifndef DOOR_MOTOR_OC
#define DOOR_MOTOR_OC 0 // 0 or 1
#endif
#define DOOR_MOTOR_OCPIN (6-DOOR_MOTOR_OC)
#ifndef DOOR_MOTOR_TIMER_DIV
#define DOOR_MOTOR_TIMER_DIV 1024
#endif
#define DOOR_MOTOR_TIMER_SCALE CONCAT(TIMER_SCALE_DIV_,DOOR_MOTOR_TIMER_DIV)
void door_set_motor(uint8_t value) {
  if (value < 3) {
    if (value == 0) {
      // disable motor
      CONCAT(PORT,DOOR_MOTOR_PORT) |= (1 << MOTOR_DISABLE);
      Timer_SetScale(0,TIMER_SCALE_STOPPED);
    } else {
      // set direction pin
      if (value == 2)
        CONCAT(PORT,DOOR_MOTOR_PORT) |= (1 << MOTOR_DIR);
      else
        CONCAT(PORT,DOOR_MOTOR_PORT) &= ~(1 << MOTOR_DIR);
      // enable motor and run it.
      CONCAT(PORT,DOOR_MOTOR_PORT) &= ~(1 << MOTOR_DISABLE);
      Timer_SetScale(0,DOOR_MOTOR_TIMER_SCALE);
    }
  }
}

void door_motor_init() {
  CONCAT(PORT,DOOR_MOTOR_PORT) =
         (CONCAT(PORT,DOOR_MOTOR_PORT) & ~(1 << MOTOR_DIR))
       | (1 << MOTOR_DISABLE); // enable pin is inverted.
  CONCAT(DDR,DOOR_MOTOR_PORT) |= ((1 << MOTOR_DISABLE) | (1 << MOTOR_DIR));
  // setup the timer functionality.
  Timer_Init(0);
  OCR0A = 255;
  if (DOOR_MOTOR_OC == 1)
    OCR0B = 0;
  //Timer_SetWave(0,TIMER0_WAVE_CTC); // can change frequency with ORC0A here.
  Timer_SetWave(0,TIMER0_WAVE_NORMAL);
  uint8_t mask = DOOR_MOTOR_OC == 0 ? (1<<COM0A0) : (1<<COM0B0);
  TCCR0A |= mask; // Toggle OC0A/B on compare match
  // enable output
  DDRD |= (1 << DOOR_MOTOR_OCPIN);
}

#endif // MOTOR_STEPPER_H

