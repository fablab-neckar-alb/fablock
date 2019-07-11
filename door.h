/*

  door.h - manages two door sensors (closed, locked) and a locking motor with sense pin.

  Copyright (c) 2019 Thomas Kremer

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#ifndef __DOOR_H__
#define __DOOR_H__

// TODO: can we please migrate from B/C/D to 0/1/2? PCMSK* are numbered.
// DONE: #ifdefs
// DONE: #define DOOR_PORT D
// now we only need to define _PORT, _MOTOR_PIN1, _SENSOR_PIN and _BOLTSENSOR_PIN

// additional defines: DOOR_MOTOR_IS_STEPPER and DOOR_MOTOR_IS_SERVO
#ifndef DOOR_PORT
#define DOOR_PORT D
#endif
#ifndef DOOR_MOTOR_PORT
#define DOOR_MOTOR_PORT DOOR_PORT
#endif
#ifndef DOOR_MOTOR_PIN1
#ifdef DOOR_BASE_PIN
#define DOOR_MOTOR_PIN1 DOOR_BASE_PIN
#else
#define DOOR_MOTOR_PIN1 2
#endif
#endif
#ifndef DOOR_MOTOR_PIN2
#define DOOR_MOTOR_PIN2 (DOOR_MOTOR_PIN1+1)
#endif

#ifndef DOOR_SENSOR_PORT
#define DOOR_SENSOR_PORT DOOR_PORT
#endif
#ifndef DOOR_SENSOR_PIN
#define DOOR_SENSOR_PIN (DOOR_MOTOR_PIN2+1)
#endif

#ifndef DOOR_BOLTSENSOR_PORT
#define DOOR_BOLTSENSOR_PORT DOOR_PORT
#endif
#ifndef DOOR_BOLTSENSOR_PIN
#define DOOR_BOLTSENSOR_PIN (DOOR_SENSOR_PIN+1)
#endif

#ifndef CONCAT
#define CONCAT2(x,y) x##y
#define CONCAT(x,y) CONCAT2(x,y)
#endif

/* TODO: implement sense:
  - motor voltage supply is behind a resistor (5.6 Ohm) and attached to ADC7.
  - read motor voltage while in use to detect stalling.
  Voltages (measured):
    idle: 4.86V
    free running: 3.96V
    heavy duty: 3.4V
    stall: 2.82V
*/

// TODO: haven't used minlocktime yet
/*
  max = after that time, it makes no sense to move any further, even if we
        haven't succeeded locking/unlocking.
  min = if the sensor indicates success within that time it must be a
        false positive.
  over = after the sensor indicates success, we move further that long, to be
         sure.
  retry = if we didn't succeed locking the door, we should definitely try
          again that much later, when the motor has cooled down a bit.
  close = if the door just closed, wait this long to lock it.
*/
#define DOOR_MINLOCKTIME msec2ticks(2000,TIMER_DIV)
//#define DOOR_MAXLOCKTIME msec2ticks(6000,TIMER_DIV)
#define DOOR_MAXLOCKTIME msec2ticks(4*6000,TIMER_DIV)
#define DOOR_OVERLOCKTIME msec2ticks(4000,TIMER_DIV)
#define DOOR_MINUNLOCKTIME msec2ticks(2000,TIMER_DIV)
//#define DOOR_MAXUNLOCKTIME msec2ticks(6000,TIMER_DIV)
#define DOOR_MAXUNLOCKTIME msec2ticks(4*6000,TIMER_DIV)
//#define DOOR_OVERUNLOCKTIME msec2ticks(1000,TIMER_DIV)
#define DOOR_OVERUNLOCKTIME msec2ticks(3*6000,TIMER_DIV)
#define DOOR_RETRYLOCKTIME sec2ticks(20,TIMER_DIV)
#define DOOR_CLOSELOCKTIME sec2ticks(2,TIMER_DIV)
#define DOOR_UNLOCKLOCKTIME sec2ticks(8,TIMER_DIV)

#define DOOR_MOTORFAIL_STALL_TIME msec2ticks(500,TIMER_DIV)
#define DOOR_MOTORFAIL_IDLE_TIME msec2ticks(2000,TIMER_DIV)
#define DOOR_MOTORFAIL_RUNNING_TIME msec2ticks(2000,TIMER_DIV)

#ifndef DOOR_MOTOR_SENSE_PIN
#define DOOR_MOTOR_SENSE_PIN 7
#endif
// stall limit: below 3V is stall. We should stop the motor and try again later. (TODO)
//#define DOOR_MOTOR_SENSE_VOLTAGE_STALL ((int16_t)(3*1024/5))
// above 4.2V is not running. If we read this while the motor is enabled, we should report a motor error. (TODO)
//#define DOOR_MOTOR_SENSE_VOLTAGE_RUNNING ((int16_t)(4.2*1024/5))
//#define DOOR_MOTOR_SENSE_VOLTAGE_RUNNING 930
// measuring with voltmeter was not really relevant for the real scenario.

// estimated by measuring with MC:
// grep -a "^ *[0-9]\+$" stalling.txt | sort -n | uniq -c
#define DOOR_MOTOR_SENSE_VOLTAGE_RUNNING 930
#define DOOR_MOTOR_SENSE_VOLTAGE_STALL 700
#define DOOR_MOTOR_SENSE_FUZZ 10
//((int16_t)(0.2*1024/5))
uint8_t door_mode = 0;

bool door_is_locked() {
  return CONCAT(PIN,DOOR_BOLTSENSOR_PORT) & (1 << DOOR_BOLTSENSOR_PIN);
}

bool door_is_closed() {
  return CONCAT(PIN,DOOR_SENSOR_PORT) & (1 << DOOR_SENSOR_PIN);
}

/*
  0 = stop
  1 = forward
  2 = backward
  everything else is invalid
*/
void door_set_motor(uint8_t value);

#ifdef DOOR_MOTOR_IS_SERVO
void door_set_motor(uint8_t value) {
  if (value < 3) {
    if (value == 0) {
      // disable motor
      servo_stop();
    } else {
      // set direction pin
      if (value == 2)
        servo_set_pos(0);
      else
        servo_set_pos(255);
      // enable motor and run it.
      // TODO: should we really make a servo_maybe_start()?
      // TODO: should we really rely on servo_stop() to be idempotent?
      servo_stop();
      servo_start();
    }
  }
}

void door_motor_init() {
  servo_init();
}

#elif defined(DOOR_MOTOR_IS_STEPPER)
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
      CONCAT(PORT,DOOR_MOTOR_PORT) |= (1 << DOOR_MOTOR_PIN1);
      Timer_SetScale(0,TIMER_SCALE_STOPPED);
    } else {
      // set direction pin
      if (value == 2)
        CONCAT(PORT,DOOR_MOTOR_PORT) |= (1 << DOOR_MOTOR_PIN2);
      else
        CONCAT(PORT,DOOR_MOTOR_PORT) &= ~(1 << DOOR_MOTOR_PIN2);
      // enable motor and run it.
      CONCAT(PORT,DOOR_MOTOR_PORT) &= ~(1 << DOOR_MOTOR_PIN1);
      Timer_SetScale(0,DOOR_MOTOR_TIMER_SCALE);
    }
  }
}

void door_motor_init() {
  CONCAT(PORT,DOOR_MOTOR_PORT) =
         (CONCAT(PORT,DOOR_MOTOR_PORT) & ~(1 << DOOR_MOTOR_PIN2))
       | (1 << DOOR_MOTOR_PIN1); // enable pin is inverted.
  CONCAT(DDR,DOOR_MOTOR_PORT) |= ((1 << DOOR_MOTOR_PIN1) | (1 << DOOR_MOTOR_PIN2));
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

#else
//#ifndef DOOR_MOTOR_IS_STEPPER
void door_set_motor(uint8_t value) {
  if (value < 3) {
    CONCAT(PORT,DOOR_MOTOR_PORT) =
      (CONCAT(PORT,DOOR_MOTOR_PORT)
       & ~((1 << DOOR_MOTOR_PIN1) | (1 << DOOR_MOTOR_PIN2)))
      | (value == 1 ? (1 << DOOR_MOTOR_PIN1) : value == 2 ? (1 << DOOR_MOTOR_PIN2) : 0);
  }
}

void door_motor_init() {
  CONCAT(PORT,DOOR_MOTOR_PORT) &= ~((1 << DOOR_MOTOR_PIN1) | (1 << DOOR_MOTOR_PIN2));
  CONCAT(DDR,DOOR_MOTOR_PORT) |= ((1 << DOOR_MOTOR_PIN1) | (1 << DOOR_MOTOR_PIN2));
}
#endif

void door_init() {
  door_mode = 0;
  CONCAT(DDR,DOOR_BOLTSENSOR_PORT) &= ~(1 << DOOR_BOLTSENSOR_PIN);
  CONCAT(DDR,DOOR_SENSOR_PORT) &= ~(1 << DOOR_SENSOR_PIN);
  CONCAT(PORT,DOOR_BOLTSENSOR_PORT) |= (1 << DOOR_BOLTSENSOR_PIN);
  CONCAT(PORT,DOOR_SENSOR_PORT) |= (1 << DOOR_SENSOR_PIN);

  door_motor_init();
}

void EVENT_door_locked(bool success);
void EVENT_door_unlocked(bool success);

void door_lock();

void door_enter_mode(uint8_t mode) {
  //uint8_t old_mode = door_mode;
  door_mode = mode;
  door_set_motor(mode);
  uint8_t mask = (1 << DOOR_MOTOR_SENSE_PIN);
  if (mode == 0) {
    // we're not watching the motor when it's stopped.
    adc_watch_set_mask(adcw_state.mask & ~mask);
  } else {
    // motor running. Watch it.
    adc_watch_set_range(DOOR_MOTOR_SENSE_PIN,
      DOOR_MOTOR_SENSE_VOLTAGE_STALL,DOOR_MOTOR_SENSE_VOLTAGE_RUNNING);
    adc_watch_set_mask(adcw_state.mask | mask);
  }
}

void EVENT_door_motor_failing(uint8_t);

/*
  1 = stall (before succeeding in whatever we're doing)
  2 = not running, but should
  3 = running, but should not
*/

void door_maybe_motorfail_event(void* param) {
  uint8_t reason = (uint16_t)param;

  if (reason == 1) {
    // motor stalls. Stop motor immediately.
    // Maybe we hit an end (check by locking switch).
    // Otherwise bad, notify user.
    uint8_t mode = door_mode;
    door_enter_mode(0);
    if (mode == 0 || // The motor should never stall when it is off.
        (mode == 1 && !door_is_locked()) || // locking failed
        (mode == 2 && door_is_locked())) // unlocking failed
      EVENT_door_motor_failing(reason);
    if (mode == 1)
      EVENT_door_locked(door_is_locked());
    else if (mode == 2)
      EVENT_door_unlocked(!door_is_locked());
  } else if (reason == 2) {
    // motor is not running, but should be. Notify user.
    if (door_mode != 0)
      EVENT_door_motor_failing(reason);
  } else if (reason == 3) {
    // motor is running, but shouldn't be. Notify user.
    if (door_mode == 0)
      EVENT_door_motor_failing(reason);
  }
}

void door_on_motor_sense_read(int16_t value) {
  uint8_t reason = 0;
  int16_t low,high;
  uint32_t dtime;
  if (value < DOOR_MOTOR_SENSE_VOLTAGE_STALL) {
    reason = 1;
    low = 0;
    high = DOOR_MOTOR_SENSE_VOLTAGE_STALL+DOOR_MOTOR_SENSE_FUZZ;
    dtime = DOOR_MOTORFAIL_STALL_TIME;
  } else if (value > DOOR_MOTOR_SENSE_VOLTAGE_RUNNING) {
    reason = 2;
    low = DOOR_MOTOR_SENSE_VOLTAGE_RUNNING-DOOR_MOTOR_SENSE_FUZZ;
    high = 1023;
    dtime = DOOR_MOTORFAIL_IDLE_TIME;
  } else {
    reason = 3;
    low = DOOR_MOTOR_SENSE_VOLTAGE_STALL-DOOR_MOTOR_SENSE_FUZZ;
    high = DOOR_MOTOR_SENSE_VOLTAGE_RUNNING+DOOR_MOTOR_SENSE_FUZZ;
    dtime = DOOR_MOTORFAIL_RUNNING_TIME;
  }
//  low = 1023;
//  high = 0;
  adc_watch_set_range(DOOR_MOTOR_SENSE_PIN,low,high);
  dequeue_events(&door_maybe_motorfail_event);
  if ( reason == 1 ||
       (reason == 2 && door_mode != 0) ||
       (reason == 3 && door_mode == 0)
     ) {
    enqueue_event_rel(dtime,&door_maybe_motorfail_event,(void*)(uint16_t)reason);
  }
}  
/*
  if (value < DOOR_MOTOR_SENSE_VOLTAGE_STALL) {
    // motor stalls. Stop motor immediately.
    // Maybe we hit an end (check by locking switch).
    // Otherwise bad, notify user.
    dequeue_events(&door_maybe_motorfail_event);
    enqueue_event_rel(DOOR_MOTORFAIL_TIME,&door_maybe_motorfail_event,(void*)1);

    uint8_t mode = door_mode;
    door_enter_mode(0);
    if (mode == 0 || // The motor should never stall when it is off.
        (mode == 1 && !door_is_locked()) || // locking failed
        (mode == 2 && door_is_locked())) // unlocking failed
      EVENT_door_motor_failing(1);
    //adc_watch_set_range(DOOR_MOTOR_SENSE_PIN,0,
    //  DOOR_MOTOR_SENSE_VOLTAGE_STALL);
  } else if (value > DOOR_MOTOR_SENSE_VOLTAGE_RUNNING) {
    // motor is not running, but should be. Notify user.
    adc_watch_set_range(DOOR_MOTOR_SENSE_PIN,
      DOOR_MOTOR_SENSE_VOLTAGE_RUNNING,1023);
    if (door_mode != 0)
      EVENT_door_motor_failing(2);
  } else {
    // motor is running, but shouldn't be. Notify user.
    if (door_mode == 0)
      EVENT_door_motor_failing(3);
    adc_watch_set_range(DOOR_MOTOR_SENSE_PIN,
      DOOR_MOTOR_SENSE_VOLTAGE_STALL,DOOR_MOTOR_SENSE_VOLTAGE_RUNNING);
  }
}
*/

void door_lock_event(void* param) {
  if (param == NULL) {
    uint8_t mode = door_mode;
    door_enter_mode(0);
    if (mode == 1 && !door_is_locked() && door_is_closed()) {
      // retry later.
      enqueue_event_rel(DOOR_RETRYLOCKTIME,&door_lock_event,(void*)1);
    }
    if (mode == 1)
      EVENT_door_locked(door_is_locked());
    else if (mode == 2)
      EVENT_door_unlocked(!door_is_locked());
  } else if (param == (void*)1) {
    // retry.
    door_lock();
  }
}

void door_schedule_locking(uint32_t reltime) {
  dequeue_events(&door_lock_event);
  door_enter_mode(0);
  enqueue_event_rel(reltime,&door_lock_event,(void*)1);
}

void door_lock() {
  // turn motor until sensor says yo plus delta.
  dequeue_events(&door_lock_event);
  door_enter_mode(1);
  enqueue_event_rel(DOOR_MAXLOCKTIME,&door_lock_event,NULL);
}

void door_unlock() {
  // turn motor back until sensor says yo plus delta.
  dequeue_events(&door_lock_event);
  door_enter_mode(2);
  enqueue_event_rel(DOOR_MAXUNLOCKTIME,&door_lock_event,NULL);
}

// TODO: who's responsible for interrupt masks?
// to be called from pin-change interrupt handler
// TODO: is lock-on-close business-logic and thus responsibility of the user?
void door_sensor_changed() {
  if (door_is_closed()) {
/*    dequeue_events(&door_lock_event);
    door_mode = 0;
    door_set_motor(0);
    enqueue_event_rel(DOOR_CLOSELOCKTIME,&door_lock_event,(void*)1);
*/
    door_schedule_locking(DOOR_CLOSELOCKTIME);
  }
}

// to be called from pin-change interrupt handler
void door_boltsensor_changed() {
  if (door_mode == 1 && door_is_locked()) {
    dequeue_events(&door_lock_event);
    enqueue_event_rel(DOOR_OVERLOCKTIME,&door_lock_event,NULL);
  } else if (door_mode == 2 && !door_is_locked()) {
    dequeue_events(&door_lock_event);
    enqueue_event_rel(DOOR_OVERUNLOCKTIME,&door_lock_event,NULL);
  } else if (door_mode == 0 && !door_is_locked()) {
    door_schedule_locking(DOOR_UNLOCKLOCKTIME);
  }
}


#endif
