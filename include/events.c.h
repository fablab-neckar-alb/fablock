/*

  High-level timer event subsystem.
  Manages a queue of timed events and a time source measured in ticks-since-reset. Consumes Timer 1.

  Copyright (c) 2018-2020 Thomas Kremer

*/

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 */

#include <events.h>
#include <timers.h>

/**
 * @brief Computes the successor of a value in modular arithmetic.
 * 
 * Increments the value `i` within the range [0, mod - 1]. 
 * If `i` is less than `mod - 1`, returns `i + 1`; otherwise, returns 0.
 * 
 * @param i   Current value (8-bit unsigned integer, must satisfy 0 <= i < mod.
 * @param mod Modulus defining the range (8-bit unsigned integer, must be > 0).
 * 
 * @return uint8_t The next value in the cycle.
 */
static inline uint8_t succmod(uint8_t i, uint8_t mod)
{
  return (i!=mod-1)?i+1:0;
}

/**
 * @brief Computes the predecessor of a value in modular arithmetic.
 * 
 * Decrements the value `i` within the range [0, mod - 1]. 
 * If `i` is greater than 0, returns `i - 1`; otherwise, wraps around to `mod - 1`.
 * 
 * @param i   Current value (8-bit unsigned integer). Valid range: 0 <= i < mod.
 * @param mod Modulus defining the range (8-bit unsigned integer, must be > 0).
 * 
 * @return uint8_t The previous value in the cycle.
 */
static inline uint8_t predmod(uint8_t i, uint8_t mod)
{
  return (i!=0)?i-1:mod-1;
}

/*
  Macro definitions to so that it is not needed to write x = succmod(x,(mod));
*/
#define events_incmod(x,mod) do { x = succmod(x,(mod)); } while(0)
#define events_decmod(x,mod) do { x = predmod(x,(mod)); } while(0)

// instead of x > y. Means that x >= y > x-(1<<(sizeof(x)-1))
// <=> (x-y >= 0) for signed types:
#define gteq_mod_type(x,y) (((x) - (y)) & (1 << (sizeof(x)*8-1)) == 0)

static inline bool gteq_mod32(uint32_t x,uint32_t y) {
  return (int32_t)(x-y) >= 0;
}

static inline bool gteq_mod16(uint16_t x,uint16_t y) {
  return (int16_t)(x-y) >= 0;
}

static inline bool gteq_mod8(uint8_t x,uint8_t y) {
  return (int8_t)(x-y) >= 0;
}

static inline void events_checknclear_ovf(void) {
  bool tovf;
  tovf = (Timer_Interrupt_Flags(1) & TIMER_INTERRUPT_OVERFLOW) != 0;
  if (tovf) {
    Timer_Interrupt_Flag_Clear(1,TIMER_INTERRUPT_OVERFLOW);
    event_time_high++;
  }
}

/*
  Here be dragon races:
    Even with cli() we cannot (and don't want to) stop the hardware timer
    from increasing and possibly overflowing.
    We cannot get the value and the overflow flag at the same time
    (but would need to), so we get the flag before and after getting the value,
    and check, if it changed in between. If it did, we cannot determine whether
    we got the value before or after the overflow, so we assume it is 0 and
    after the overflow.
    [Due to a design flaw we cannot catch an overflow interrupt before an
    output-compare interrupt anyway.]

  This function takes min 26 cycles and max 37 cycles. (incl. call & ret)

*/
static /*inline*/ uint32_t get_time_sync(void) {
  uint16_t l,h;
  bool tov_before, tov_after;
  // 5 cycles (call).
  // 4 cycles (2*lds):
  h = event_time_high;
  // 1 cycle (in):
  tov_before = (Timer_Interrupt_Flags(1) & TIMER_INTERRUPT_OVERFLOW) != 0;
  // 4 cycles (2*lds):
  l = Timer_Value(1);
  // 3/2 cycles ((1/2/3)+(2/0): sbis+rjmp):
  tov_after = (Timer_Interrupt_Flags(1) & TIMER_INTERRUPT_OVERFLOW) != 0;
  if (tov_after) {
    // 2 cycles (subi+subci):
    h++;
    // we update time_high, as it does not take much time (6 cycles) and
    // is rare.
    // 4 cycles (2*sts):
    event_time_high = h;
    // 2 cycles (ldi+out):
    Timer_Interrupt_Flag_Clear(1,TIMER_INTERRUPT_OVERFLOW);
    // 3/2 cycles ((1/2/3)+(2/0): sbrc+rjmp):
    if (!tov_before) {
      // 2 cycles (2*ldi):
      l = 0;
    }
  }
  register uint32_t res __asm__("r22"); // reduces shuffling around of bytes.
  // 2 cycles (movw):
  __asm__ (
    "movw %0, %1" "\n\t"
//    "mov %A0, %A1" "\n\t"
//    "mov %B0, %B1" "\n\t"
    : "=r" (res)
    : "r" (l)
  );
  // 2 cycles (movw):
  __asm__ (
    "movw %C0, %1" "\n\t"
//    "mov %C0, %A1" "\n\t"
//    "mov %D0, %B1" "\n\t"
    : "+r" (res)
    : "r" (h)
  );
  // 5 cycles (ret):
  return res;
// This would do heavy calculating:
//  return ((uint32_t)h) << 16 | l;

// This is much better & tighter. But the compiler messes up the optimization
// by shuffling along with the registers heavily.
//... ok, finally it got the idea of the above...
// yet still it doesn't manage the "a << 16 | b" notation...
/*
  h = event_time_high;
  __asm__ (
      // Load flags
      "in __tmp_reg__, %2" "\n\t"
      // Load value
      "lds %A0, %3" "\n\t"
      "lds %B0, %3+1" "\n\t"
      // if ovf flag still cleared, we're done
      "sbis %2, %4" "\n\t"
      "rjmp L_%=" "\n\t"
      // inc(h)
      "adiw %1, 1" "\n\t"
      // if ovf flag was already set before, we're done
      "sbrc __tmp_reg__,%4" "\n\t"
      "rjmp L_%=" "\n\t"
      // ovf flag just turned on. Clear l.
      "clr %A0" "\n\t"
      "clr %B0" "\n\t"
      "L_%=:" "\n\t"
    :
      "=r" (l),
      "=w" (h)
    :
      "I" (_SFR_IO_ADDR(Timer_Interrupt_Flags(1))),
      "M" (_SFR_MEM_ADDR(Timer_Value(1))),
      "I" (0),// ( == lb(TIMER_INTERRUPT_OVERFLOW))
      "1" (h)
  );
  
  return ((uint32_t)h) << 16 | l;*/
}

uint32_t get_time(void) {
  uint32_t res;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    res = get_time_sync();
  }
  return res;
}

/*
ISR (TIMER1_OVF_vect, ISR_BLOCK)
{
  uint16_t time_high = event_time_high;
  time_high++;
  event_time_high = time_high;
  if (event_first_ix != EVENT_INDEX_EMPTY) {
    uint32_t time = event_queue[event_first_ix].time;
    if (time_high == time >> 16) {
      OCR1A = time & 0xffff;
      Timer_Interrupt_Enable(1,TIMER_INTERRUPT_OUTPUT_COMPARE_A);
    }
  }
}
*/

ISR (TIMER1_COMPA_vect, ISR_BLOCK)
{
  do {
    uint8_t first_ix = event_first_ix;
    if (first_ix == EVENT_INDEX_EMPTY) {
      events_checknclear_ovf();
      return;
    }
    event_t *ev = &event_queue[first_ix];
    uint32_t next_t = ev->time;
    uint32_t now = get_time_sync();
    if (gteq_mod32(now,next_t)) {
      uint8_t free_ix = event_free_ix;
      first_ix++;
      event_t *ev_next = ev+1;
      if (first_ix == EVENT_QUEUE_SIZE) {
        first_ix = 0;
        ev_next = &event_queue[0];
      }
      if (first_ix != free_ix) {
        uint16_t t_lo = ev_next->time /*event_queue[first_ix].time*/ & 0xffff;
        OCR1A = t_lo;
        // 4 cycles (2*lds):
        uint16_t now = Timer_Value(1);
        // 2 cycles for useless movw.
        // 5/4 cycles (1+1+(1/2)+(2/0) sub+sbc+sbrc+rjmp)
        if (gteq_mod16(now,t_lo)) {
          // 6 cycles (1+1+4 subi+sbci+2*sts)
          OCR1A = now+18;
        }
      } else {
        first_ix = EVENT_INDEX_EMPTY;
      }
      event_first_ix = first_ix;
      event_handler_fun_t h = ev->handler;
      void *p = ev->param;
      h(p);
      cli();
      // h() may sei() and even modify events.
    } else {
//  This could strengthen our ovf detection:
//      OCR1A ^= 0x8000;
      return;
    }
  } while (1);
}


/**
 * @brief Sets the hardware timer to fire at the given time or as soon as possible if the time has already passed.
 * 
 * This function ensures that the hardware timer is set correctly, handling potential race conditions that may occur
 * when the specified `time` has already passed. The function adjusts the timer to avoid missing events and ensures
 * accurate timing by accounting for potential delays caused by the system state and interrupt timing.
 * 
 * This function should only be called with interrupts **disabled** to prevent race conditions during the timer setup.
 *
 * @param time The desired time (32-bit value) for the hardware timer in cycles.
 * 
 * @note This function is intended for internal use and ensures that the timer is set at the correct time or adjusted
 *       in case the specified time is in the past. The time will be updated based on the current system time and
 *       timer scaling, ensuring that the timer is not missed.
 *
 * @details 
 * - If the specified `time` has already passed, the function reschedules the timer for the nearest valid time 
 *   using the current system time and an appropriate delay (`dist`).
 * - The function accounts for race conditions by reading the current time (`get_time_sync()`), adjusting the timer
 *   value accordingly, and ensuring that the timer interrupt will fire correctly once enabled.
 * - The timing calculation includes several optimizations to handle different timer scaling options.
 * - The function is optimized for minimal overhead to avoid introducing additional latency.
 * 
 * @warning This function modifies the hardware timer directly and should only be called with interrupts disabled
 *          to ensure consistent behavior.
 */
static inline void events_set_hw_timer(uint32_t time)
{
  OCR1A = time & 0xffff;
  // This is a race. We need to make sure time > get_time_sync() at the
  // time we set OCR1A. Otherwise we might be a whole cycle late.
  // altogether: 71/72/73 cycles (37 + 17 + (5/7/8) + 12).
  // Below dist is chosen so that dist >= ceil(cycles/DIV)+1.
  // 37 cycles (call-ret):
  uint32_t now = get_time_sync();
  // 4 cycles for 2*movw.
  // another 4 cycles for 2*movw.
  // 4 cycles for sub+3*sbc,
  // 3/2 cycles ((1/2/3)+(2/0) sbrc+rjmp):
  if (gteq_mod32(now,time)) { // The event is in the past. Reschedule.
    // 3 cycles (lds+andi):
    uint8_t scale = (TCCR1B >> CS10) & 7;
    uint8_t dist;
    // whole if-clause: 5/7/8 cycles.
    // 3/2 cycles (1+(1/2) cpi+brne):
    if (scale == TIMER_SCALE_DIV_8)
      // 3 cycles (ldi+rjmp)
      dist = 10;
    // 2/3 cycles (1+(1/2) cpi+breq):
    else if (scale == TIMER_SCALE_1)
      // 1 cycle (ldi)
      dist = 90; //73, but just to make sure.
    // 3 cycles (ldi+rjmp)
    else dist = 3;
    // 4 cycles for pointless 2*movw.
    // 4 cycles (add+3*adc):
    time = now+dist;
    // 4 cycles (2*sts):
    OCR1A = time;
    // At this point the race ends. When time moves on, the interrupt flag
    // will fire and as soon as we sei(), so will the interrupt vector.
  }
}

/**
 * @brief Enqueues an event using the provided event structure.
 * 
 * This function wraps the `enqueue_event_abs` function, allowing you to enqueue an event by passing an `event_t` 
 * structure. It extracts the event time, handler, and parameters from the structure and enqueues the event 
 * in the event queue.
 * 
 * @param ev  A pointer to the event structure containing the event details (time, handler, and parameters).
 * 
 * @return true if the event was successfully enqueued, false if the event queue is full.
 *
 * @note This function simplifies event enqueueing by allowing direct use of the `event_t` structure.
 */
bool enqueue_event(const event_t* ev)
{
  return enqueue_event_abs(ev->time,ev->handler,ev->param);
}

/**
 * @brief Enqueues an event at a specified absolute time in the event queue.
 * 
 * This function adds an event to the event queue at a specified absolute time. It ensures that events are added in 
 * chronological order by adjusting the queue when necessary, handling time wraparound, and updating the first 
 * available index and the event handler.
 * 
 * The function uses atomic blocks to ensure thread-safety while modifying shared variables, and handles edge cases 
 * like queue wraparound and overflow.
 *
 * @param time   The absolute time at which the event should occur (32-bit).
 * @param h      The event handler function to be called at the specified time.
 * @param param  A pointer to the parameters that should be passed to the event handler.
 * 
 * @return true if the event was successfully enqueued, false if the event queue is full.
 *
 * @note This function assumes that the event queue is managed in a circular buffer and that events are sorted 
 *       by time.
 */
bool enqueue_event_abs(uint32_t time, event_handler_fun_t h, void* param)
{
  uint8_t ix;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    // need to copy the volatiles for the optimizer:
    uint8_t first_ix = event_first_ix;
    uint8_t free_ix = event_free_ix;
    if (free_ix == first_ix)
      return false;
    if (first_ix == EVENT_INDEX_EMPTY) {
      first_ix = 0;
      free_ix = 1;
      ix = 0;
      event_first_ix = first_ix;
    } else {
      uint8_t k = predmod(free_ix,EVENT_QUEUE_SIZE);
      uint8_t low = predmod(first_ix,EVENT_QUEUE_SIZE);
      
      // we have to care about time wraparound here, so we subtract the
      // current time (coarsely) before comparing.
      int32_t time_base = (int32_t)event_time_high<<16; //get_time, more coarse, but faster.
      int32_t dtime = (int32_t)time-time_base;
      while (dtime < (int32_t)event_queue[k].time-time_base && k != low) {
        events_decmod(k,EVENT_QUEUE_SIZE);
      }
      events_incmod(k,EVENT_QUEUE_SIZE);
      for (uint8_t i = free_ix; i != k; i = (i!=0)?i-1:EVENT_QUEUE_SIZE-1) {
        event_queue[i] = event_queue[predmod(i,EVENT_QUEUE_SIZE)];
      }
      ix = k;
      events_incmod(free_ix,EVENT_QUEUE_SIZE);
    }
    if (ix == first_ix) {
      events_set_hw_timer(time);
    }
    event_free_ix = free_ix;
    event_queue[ix].time = time;
    event_queue[ix].handler = h;
    event_queue[ix].param = param;
  }
  return true;
}

/**
 * @brief Enqueues an event at a relative time offset from the current time.
 * 
 * This function calculates the absolute time by adding the specified relative time offset to the current time,
 * and then enqueues the event at the calculated time using `enqueue_event_abs`.
 * 
 * @param time   The relative time offset (in cycles) from the current time when the event should occur.
 * @param h      The event handler function to be called at the specified time.
 * @param param  A pointer to the parameters that should be passed to the event handler.
 * 
 * @return true if the event was successfully enqueued, false if the event queue is full.
 *
 * @note This function is useful for scheduling events to occur after a certain delay from the current time.
 */
bool enqueue_event_rel(uint32_t time, event_handler_fun_t h, void* param)
{
  return enqueue_event_abs(get_time()+time,h,param);
}

/**
 * @brief Removes all events with the specified handler from the event queue.
 * 
 * This function iterates through the event queue and removes all events that have the specified handler `h`. 
 * The queue is adjusted accordingly, and the first event time is updated if necessary.
 * 
 * @param h  The event handler function whose associated events should be removed.
 * 
 * @return true if one or more events were removed; false if no matching events were found.
 *
 * @note This function modifies the event queue in place and handles the update of the event indices. It also 
 *       ensures that the hardware timer is updated if the first event is removed.
 */
bool dequeue_events(event_handler_fun_t h)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    // need to copy the volatiles for the optimizer:
    uint8_t first_ix = event_first_ix;
    uint8_t free_ix = event_free_ix;
    if (first_ix == EVENT_INDEX_EMPTY) {
      return false;
    } else {
      bool deleted_first = false;
      uint8_t deleted = 0;
      for (uint8_t k = first_ix; k != free_ix;
                   k = k!=EVENT_QUEUE_SIZE-1?k+1:0) {
        if (event_queue[k].handler == h) {
          if (k == first_ix) {
            first_ix = succmod(k,EVENT_QUEUE_SIZE);
            deleted_first = true;
          } else {
            deleted++;
          }
        } else if (deleted != 0) {
          event_queue[(k+EVENT_QUEUE_SIZE-deleted)%EVENT_QUEUE_SIZE] = event_queue[k];
        }
      }
      if (deleted != 0) {
        free_ix = (free_ix+EVENT_QUEUE_SIZE-deleted)%EVENT_QUEUE_SIZE;
        event_free_ix = free_ix;
      }
      if (deleted_first) {
        if (first_ix == free_ix) {
          first_ix = EVENT_INDEX_EMPTY;
          // we don't need to update OCR1A. The old value will do.
        } else {
          uint32_t time = event_queue[first_ix].time;
          events_set_hw_timer(time);
        }
        event_first_ix = first_ix;
      }
      bool res = deleted_first || (deleted != 0);
      return res;
    }
  }
  return false; // never reached, but compiler disagrees.
}

/**
 * @brief Initializes and starts the event system with a specified timer scale.
 * 
 * This function initializes Timer 1, sets the output compare register `OCR1A` based on the first event's time 
 * (if available), and configures the timer interrupts. It also sets the timer's scaling factor as specified by 
 * the `scale` parameter.
 * 
 * @param scale  The timer scaling factor to control the timer's frequency.
 *
 * @note This function assumes that the event system is already initialized and that the first event's time 
 *       is available in the event queue. It configures the timer for event-based scheduling.
 */
void events_start(uint8_t scale)
{
  Timer_Init(1);
  if (event_first_ix != EVENT_INDEX_EMPTY)
    OCR1A = event_queue[event_first_ix].time & 0xffff;
   else OCR1A = 0;
  Timer_Interrupts(1) = TIMER_INTERRUPT_OUTPUT_COMPARE_A;
  Timer_SetScale(1,scale);
}
