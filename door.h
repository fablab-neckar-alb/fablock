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


#ifndef CONCAT
#define CONCAT2(x, y) x##y
#define CONCAT(x, y) CONCAT2(x, y)
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
#define DOOR_MINLOCKTIME msec2ticks(2000, TIMER_DIV)
// #define DOOR_MAXLOCKTIME msec2ticks(6000,TIMER_DIV)
#define DOOR_MAXLOCKTIME msec2ticks(3000, TIMER_DIV)
#define DOOR_OVERLOCKTIME msec2ticks(500, TIMER_DIV)
#define DOOR_MFAIL_RECOVERTIME msec2ticks(200, TIMER_DIV)
#define DOOR_MINUNLOCKTIME msec2ticks(2000, TIMER_DIV)
// #define DOOR_MAXUNLOCKTIME msec2ticks(6000,TIMER_DIV)
#define DOOR_MAXUNLOCKTIME msec2ticks(4 * 6000, TIMER_DIV)
// #define DOOR_OVERUNLOCKTIME msec2ticks(1000,TIMER_DIV)
#define DOOR_OVERUNLOCKTIME msec2ticks(3 * 6000, TIMER_DIV)
#define DOOR_RETRYLOCKTIME sec2ticks(20, TIMER_DIV)
#define DOOR_CLOSELOCKTIME sec2ticks(2, TIMER_DIV)
#define DOOR_UNLOCKLOCKTIME sec2ticks(8, TIMER_DIV)

#define DOOR_MOTORFAIL_STALL_TIME msec2ticks(250, TIMER_DIV)
#define DOOR_MOTORFAIL_IDLE_TIME msec2ticks(2000, TIMER_DIV)
#define DOOR_MOTORFAIL_RUNNING_TIME msec2ticks(2000, TIMER_DIV)

#ifndef DOOR_MOTOR_SENSE_PIN
#define DOOR_MOTOR_SENSE_PIN 7
#endif
// stall limit: below 3V is stall. We should stop the motor and try again later. (TODO)
// #define DOOR_MOTOR_SENSE_VOLTAGE_STALL ((int16_t)(3*1024/5))
// above 4.2V is not running. If we read this while the motor is enabled, we should report a motor error. (TODO)
// #define DOOR_MOTOR_SENSE_VOLTAGE_RUNNING ((int16_t)(4.2*1024/5))
// #define DOOR_MOTOR_SENSE_VOLTAGE_RUNNING 930
// measuring with voltmeter was not really relevant for the real scenario.

// estimated by measuring with MC:
// grep -a "^ *[0-9]\+$" stalling.txt | sort -n | uniq -c
#define DOOR_MOTOR_SENSE_VOLTAGE_RUNNING 900
#define DOOR_MOTOR_SENSE_VOLTAGE_STALL 720
#define DOOR_MOTOR_SENSE_FUZZ 10

#define DOOR_MODE_IDLE 0
#define DOOR_MODE_LOCKING 1 
#define DOOR_MODE_UNLOCKING 2
#define DOOR_MODE_LOCKRETRACT 3
#define DOOR_MODE_UNLOCKRETRACT 4

uint8_t door_mode = 0;

bool door_is_locked()
{
  return !(CONCAT(PIN, DOOR_BOLTSENSOR_PORT) & (1 << DOOR_BOLTSENSOR_PIN));
}

bool door_is_closed()
{
  return !(CONCAT(PIN, DOOR_SENSOR_PORT) & (1 << DOOR_SENSOR_PIN));
}

/*
  0 = stop
  1 = forward
  2 = backward
  everything else is invalid
*/
void door_set_motor(uint8_t dir);
void door_schedule_mfail_recover(uint8_t mode);


#if defined(MOTOR_IS_DC)
#ifdef MOTOR_DRIVER_HR8833
#include "motor/HR8833.h"
#endif /* MOTOR_DRIVER_HR8833 */

#elif defined(MOTOR_IS_SERVO)
#include "motor/servo.h"

#elif defined(MOTOR_IS_STEPPER)
#include "motor/stepper.h"
#else
#error "Unknown Motor Configuration"
#endif

void door_init()
{
  door_mode = DOOR_MODE_IDLE;
  CONCAT(DDR, DOOR_BOLTSENSOR_PORT) &= ~(1 << DOOR_BOLTSENSOR_PIN);
  CONCAT(DDR, DOOR_SENSOR_PORT) &= ~(1 << DOOR_SENSOR_PIN);
  CONCAT(PORT, DOOR_BOLTSENSOR_PORT) |= (1 << DOOR_BOLTSENSOR_PIN);
  CONCAT(PORT, DOOR_SENSOR_PORT) |= (1 << DOOR_SENSOR_PIN);

  door_motor_init();
}

void EVENT_door_locked(bool success);
void EVENT_door_unlocked(bool success);
void EVENT_door_mode_changed(uint8_t old_mode);
void motor_stop_event(void *param);

void door_lock();

void door_enter_mode(uint8_t mode)
{
  uint8_t old_mode = door_mode;
  door_mode = mode;
  door_set_motor(mode);
  uint8_t mask = (1 << DOOR_MOTOR_SENSE_PIN);
  if (mode == DOOR_MODE_IDLE)
  {
    // we're not watching the motor when it's stopped.
    adc_watch_set_mask(adcw_state.mask & ~mask);
  }
  else
  {
    // motor running. Watch it.
    adc_watch_set_range(DOOR_MOTOR_SENSE_PIN,
                        DOOR_MOTOR_SENSE_VOLTAGE_STALL, DOOR_MOTOR_SENSE_VOLTAGE_RUNNING);
    adc_watch_set_mask(adcw_state.mask | mask);
  }
  EVENT_door_mode_changed(old_mode);
}

void EVENT_door_motor_failing(uint8_t);

/*
  1 = stall (before succeeding in whatever we're doing)
  2 = not running, but should
  3 = running, but should not
*/
#define MOTOR_SENSE_EVENT_REASON_stall 1
#define MOTOR_SENSE_EVENT_REASON_not_running_but_should 2
#define MOTOR_SENSE_EVENT_REASON_running_but_should_not 3
void door_maybe_motorfail_event(void *param)
{
  uint8_t reason = (uint16_t)param;

  if (reason == MOTOR_SENSE_EVENT_REASON_stall)
  {
    // motor stalls. Stop motor immediately.
    // Maybe we hit an end (check by locking switch).
    // Otherwise bad, notify user.
    uint8_t mode = door_mode;
    door_enter_mode(DOOR_MODE_IDLE);
    if (mode == 0 ||                        // The motor should never stall when it is off.
        (mode == 1 && !door_is_locked()) || // locking failed
        (mode == 2 && door_is_locked()))    // unlocking failed
      {
        EVENT_door_motor_failing(reason);
      }
    if (mode == DOOR_MODE_LOCKING){
      door_schedule_mfail_recover(DOOR_MODE_LOCKRETRACT);
      EVENT_door_locked(door_is_locked());
    }
    else if (mode == DOOR_MODE_UNLOCKING){
      door_schedule_mfail_recover(DOOR_MODE_UNLOCKRETRACT);
      EVENT_door_unlocked(!door_is_locked());
    }
  }
  else if (reason == MOTOR_SENSE_EVENT_REASON_not_running_but_should)
  {
    // motor is not running, but should be. Notify user.
    if (door_mode != DOOR_MODE_IDLE)
      EVENT_door_motor_failing(reason);
  }
  else if (reason == MOTOR_SENSE_EVENT_REASON_running_but_should_not)
  {
    // motor is running, but shouldn't be. Notify user.
    if (door_mode == DOOR_MODE_IDLE)
      EVENT_door_motor_failing(reason);
  }
}

void door_on_motor_sense_read(int16_t value)
{
  uint8_t reason = 0;
  int16_t low, high;
  uint32_t dtime;
  if (value < DOOR_MOTOR_SENSE_VOLTAGE_STALL)
  {
    reason = 1;
    low = 0;
    high = DOOR_MOTOR_SENSE_VOLTAGE_STALL + DOOR_MOTOR_SENSE_FUZZ;
    dtime = DOOR_MOTORFAIL_STALL_TIME;
  }
  else if (value > DOOR_MOTOR_SENSE_VOLTAGE_RUNNING)
  {
    reason = 2;
    low = DOOR_MOTOR_SENSE_VOLTAGE_RUNNING - DOOR_MOTOR_SENSE_FUZZ;
    high = 1023;
    dtime = DOOR_MOTORFAIL_IDLE_TIME;
  }
  else
  {
    reason = 3;
    low = DOOR_MOTOR_SENSE_VOLTAGE_STALL - DOOR_MOTOR_SENSE_FUZZ;
    high = DOOR_MOTOR_SENSE_VOLTAGE_RUNNING + DOOR_MOTOR_SENSE_FUZZ;
    dtime = DOOR_MOTORFAIL_RUNNING_TIME;
  }
  //  low = 1023;
  //  high = 0;
  adc_watch_set_range(DOOR_MOTOR_SENSE_PIN, low, high);
  dequeue_events(&door_maybe_motorfail_event);
  if (reason == 1 ||
      (reason == 2 && door_mode != DOOR_MODE_IDLE) ||
      (reason == 3 && door_mode == DOOR_MODE_IDLE))
  {
    enqueue_event_rel(dtime, &door_maybe_motorfail_event, (void *)(uint16_t)reason);
  }
}

/*

  if param is NULL
        motor is stopped
        check if closing check if door is (not locked) and closed
          retry again later
        also check if closing
          report locking result to the user 
        also check if opening
          report unlocking result to the user 
  if param == 1 -> retry again
   

 */
void door_lock_event(void *param)
{
  if (param == NULL)
  {
    uint8_t mode = door_mode;
    door_enter_mode(DOOR_MODE_IDLE);
    if (mode == DOOR_MODE_LOCKING && !door_is_locked() && door_is_closed())
    {
      // retry later.
      enqueue_event_rel(DOOR_RETRYLOCKTIME, &door_lock_event, (void *)1);
    }
    if (mode == DOOR_MODE_LOCKING)
      EVENT_door_locked(door_is_locked());
    else if (mode == DOOR_MODE_UNLOCKING)
      EVENT_door_unlocked(!door_is_locked());
  }
  else if (param == (void *)1)
  {
    // retry.
    if (door_is_closed())
      door_lock();
  }
}

void door_schedule_locking(uint32_t reltime)
{
  dequeue_events(&door_lock_event);
  door_enter_mode(DOOR_MODE_IDLE);
  enqueue_event_rel(reltime, &door_lock_event, (void *)1);
}

void door_lock()
{
  // turn motor until sensor says yo plus delta.
  dequeue_events(&door_lock_event);
  door_enter_mode(DOOR_MODE_LOCKING);
  enqueue_event_rel(DOOR_MAXLOCKTIME, &door_lock_event, NULL);
}

void door_unlock()
{
  // turn motor back until sensor says yo plus delta.
  dequeue_events(&door_lock_event);
  door_enter_mode(DOOR_MODE_UNLOCKING);
  enqueue_event_rel(DOOR_MAXUNLOCKTIME, &door_lock_event, NULL);
}

void door_schedule_mfail_recover(uint8_t mode){
  //after a seized motor stop schedule a short turn in the opposed direction
  dequeue_events(&door_lock_event);
  uint8_t dir = 0;
  if(mode == DOOR_MODE_LOCKRETRACT){
    dir = 2;
  }
  if(mode == DOOR_MODE_UNLOCKRETRACT){
    dir = 1;
  }
  door_set_motor(dir);
  enqueue_event_rel(DOOR_MFAIL_RECOVERTIME, &motor_stop_event, NULL);

}

void motor_stop_event(void *param){
  door_set_motor(0);

}

// TODO: who's responsible for interrupt masks?
// to be called from pin-change interrupt handler
// TODO: is lock-on-close business-logic and thus responsibility of the user?
void door_sensor_changed()
{
  if (door_is_closed())
  {
    door_schedule_locking(DOOR_CLOSELOCKTIME);
  }
  else
  {
    door_enter_mode(DOOR_MODE_IDLE);
  }
}

// to be called from pin-change interrupt handler
void door_boltsensor_changed()
{
  if (door_mode == DOOR_MODE_LOCKING && door_is_locked())
  {
    dequeue_events(&door_lock_event);
    enqueue_event_rel(DOOR_OVERLOCKTIME, &door_lock_event, NULL);
  }
  else if (door_mode == DOOR_MODE_UNLOCKING && !door_is_locked())
  {
    dequeue_events(&door_lock_event);
    enqueue_event_rel(DOOR_OVERUNLOCKTIME, &door_lock_event, NULL);
  }
  else if (door_mode == DOOR_MODE_IDLE && !door_is_locked())
  {
    door_schedule_locking(DOOR_UNLOCKLOCKTIME);
  }
}

#endif
