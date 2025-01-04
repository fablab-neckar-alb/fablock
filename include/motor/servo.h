/*

  servo.h - control 1 servo using bit-banging with the event subsystem.

  Copyright (c) 2019 Thomas Kremer

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#ifndef _MOTOR_SERVO_H__
#define _MOTOR_SERVO_H__
// data sheet:
//#define servo_pulse_low msec2ticks(1,TIMER_DIV)
//#define servo_pulse_high msec2ticks(2,TIMER_DIV)

// To test actual limits:
//#define servo_pulse_low msec2ticks(0.1,TIMER_DIV)
//#define servo_pulse_high msec2ticks(12.8,TIMER_DIV)

// tested results:
// correct range: 0.6 - 2.35 0.6..2.25 = 180°
#define servo_pulse_low msec2ticks(0.6,TIMER_DIV)
//miscalculation: #define servo_pulse_high msec2ticks(2.7,TIMER_DIV)
#define servo_pulse_high msec2ticks(2.35,TIMER_DIV)
#define servo_total_angle (180L*(235-60)/(225-60))
// 0.6 + 216/255*(2.7-0.6) = 216/255*2.1 = 2.38
#define servo_pwm_period msec2ticks(20,TIMER_DIV)

// TODO: Port hardcoded to Port C

#ifndef SERVO_PIN
#warning(Choosing default value for SERVO_PIN)
#define SERVO_PIN 5
#endif


//uint8_t servo_pos = 128;
//uint8_t servo_step = 1;
int32_t servo_pulse = (servo_pulse_low+servo_pulse_high)/2;


void servo_ontimer(void* param) {
  uint16_t val = (uint16_t)param;
  int32_t interv = 0;
  if (val == 0) {
    PORTC |= 1<<SERVO_PIN;
    interv = servo_pulse;
    val = 1;
  } else {
    PORTC &= ~(1<<SERVO_PIN);
    interv = servo_pwm_period - servo_pulse;
    val = 0;
  }
  enqueue_event_rel(interv,&servo_ontimer,(void*)val);
}

void servo_set_pos(uint8_t servo_pos)
{
  servo_pulse = servo_pulse_low + (((int32_t)servo_pos)*(servo_pulse_high-servo_pulse_low))/255; // going from -1 to 2 instead of 0 to 1. // /255;
  //servo_pulse = servo_pulse_low + (((int32_t)servo_pos - 64)*2*(servo_pulse_high-servo_pulse_low))/255; // going from -1 to 2 instead of 0 to 1. // /255;
}

void servo_init() {
  servo_set_pos(128); // middle
}

void servo_start() {
  PORTC &= ~(1<<SERVO_PIN);
  DDRC |= 1<<SERVO_PIN;
  servo_ontimer(NULL);
}

void servo_stop() {
  dequeue_events(&servo_ontimer);
  DDRC &= ~(1<<SERVO_PIN);
  PORTC |= 1<<SERVO_PIN;
}

#endif /* MOTOR_SERVO_H */
