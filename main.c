// for spi.h
#define NO_LUFA

#ifdef CONFIG_DEMO
  #include "demodoor.h"
#elif CONFIG_UPPER
  #include "oben.h"
#elif CONFIG_CELLAR
  #include "cellar.h"
#elif CONFIG_SOCIAL
  #include "sozialraum.h"
#else
  #error "Unset Config"
#endif

//#define DEBUG_DISPLAY
#define DEBUG_MOTOR_SENSE
//#define DEBUG_INTERRUPTS
#define ENABLE_EASTEREGGS
//#define ENABLE_COPYRIGHTED_EASTEREGGS


#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <timers.h>
#define EVENT_QUEUE_SIZE 8
#include <events.c.h>


  
#define TIMER_DIV 1
#define TIMER_SCALE TIMER_SCALE_1
//#define TIMER_DIV 1024
//#define TIMER_SCALE TIMER_SCALE_DIV_1024

// was used for morse code:
#define shortdelay msec2ticks(200.0,TIMER_DIV)
#define longdelay msec2ticks(800.0,TIMER_DIV)

/*
#define EEPROMFS_VARS 2
#include "eeprom.h"
eepromfs_index_t eep_index;
*/
//#define eep_segment_low 0
//#define eep_segment_high 1

#define ledpin 5
#define baudrate 9600
//#define baudrate 57600
//#define baudrate 19200
// 9600 19200 38400 57600 115200

// can have baudrates of up to 2000000 (usable!)

// some defines used by the last LUFA project, parts of which were included.
#define LEDS_LED1 (1<<ledpin)
#define LEDs_ToggleLEDs(mask) PINB = mask
#define LEDs_GetLEDs() (PORTB & LEDS_LED1)
#define LEDs_TurnOnLEDs(mask) PORTB |= mask
#define LEDs_TurnOffLEDs(mask) PORTB &= ~mask
#define ATTR_NO_INIT __attribute__((section(".noinit")))
#define ATTR_ALWAYS_INLINE __attribute__((always_inline))

#define outbuf_size 80
#include <usart.h>

//#define ADCW_READ_COUNT (1 << 6)
#define ADCW_READ_COUNT (1 << 5)

#include <adc.h>
#include <adc_watch.h>
//#include "spi.h"
#include "prng.h"

#ifdef DEBUG_DISPLAY
#include <pcd8544_display.h>
#include "testfont.h"
#endif


#include "beep.h"
//#define MELODY_DEFINE_KOROBEINIKI
#include "melody.h"
#include "adnauseam.h"
#include "korobeiniki.h"

#ifdef ENABLE_COPYRIGHTED_EASTEREGGS
#include "la_prapoutische.h"
#include "mccarthys_waltz.h"
#endif

#include <pinpad.h>

//#define DOOR_MOTOR_IS_STEPPER
#define DOOR_MOTOR_IS_DC
#define DOOR_MOTOR_TIMER_DIV 64
#define DOOR_MOTOR_OC 0


#include "door.h"

/*

  note: space for bolt detector is 15 x 20 x 35 mm^3

  door layout:

  <---------- 140 ------------> 
   .........3......6...II....   ^
   .  +----------*-+----*-+ .   |
   .  |<--- 90 --->|      | .   |
   .  |            |      | .   110
   .  |            |      | .   |
   .  |        --- |      | 5   |
   ...|.....3... * |*.3...|..   v
      |            |   *  |


       Motor    Door Switch
    GND VDD PWM VCC-PIN-GND
     |   |   |   |   |   |
red ======================
cable
                            Keypad  Speaker Lock Switch
                            (+) (-) PIN GND PIN GND
                             |   |   |   |   |   |
yellow ===========================================
cable

*/

void process_char(char c);
void process_pinpad_char(char c);

static char hexchar(uint8_t i) {
  return i < 10 ? '0'+i : i < 16 ? 'A'-10+i : 'X';
}

void buftohex(const uint8_t* src, char* dest, int count) {
  for (int i = 0; i < count; i++) {
    uint8_t val = src[i];
    dest[2*i] = hexchar(val >> 4);
    dest[2*i+1] = hexchar(val & 15);
  }
  dest[2*count] = 0;
}

void inttohex(uint32_t value, char* dest, int count) {
  for (int i = 0; i < count; i++) {
    uint8_t val = (value & 0xf);
    value >>= 4;
    dest[count-1-i] = hexchar(val);
  }
  dest[count] = 0;
}

uint32_t hex2int(const char* s) {
  uint32_t res = 0;
  for (int i = 0; s[i] != 0 && i < 20; i++) {
    char c = s[i];
    char v = c-'0';
    if (v > 9) v = c-'A'+10;
    if (v > 15) v = c-'a'+10;
    if (v > 15 || v < 0) v = 0;
    res <<= 4;
    res |= v;
  }
  return res;
}

bool snprintl(char* s, int len, int32_t value) {
  bool positive = value >= 0;
  bool ok = 1;
  uint32_t val;
/*  if (!positive) {
    val = -value;
  }*/
  if (positive) {
    val = value;
  } else {
    val = (uint32_t)(-value);
  }
  int i = 0;
  while (i < len && val != 0) {
    char v0 = val % 10;
    val /= 10;
    s[len-1-i] = '0'+v0;
    i++;
  }
  if (i == 0) {
    s[len-1] = '0';
    i++;
  }
  if (!positive) {
    if (i == len-1) {
      i--;
      ok = 0;
    }
    s[len-1-i] = '-';
  }
  if (val != 0) {
    ok = 0;
  }
  while (i < len) {
    s[len-1-i] = ' ';
    i++;
  }
  return ok;
}

// --- power management ---

void poweroff() {
  cli();
  events_clear();
  PORTB &= ~(1<<ledpin);
  while(true) {
    // disable interrupts
    cli();
    // disable watchdog
    wdt_reset();
    MCUSR &= ~(1 << WDRF);
    wdt_disable();
    // disable Analog Comparator (to make sure that internal AREF is not used)
    ACSR = 1 << ACD;
    // disable ADC (ref. says "The ADC must be disabled before shutdown")
    // also disable all digital input buffers on ADC pins. Should happen
    // automatically anyway, but better to be explicit.
    adc_conf(false,0xff,0);
    // shut down all peripherals
    power_all_disable();
    // go to sleep
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_bod_disable();
    sleep_cpu();
    // should not be reached. But who knows for sure?
    PORTB |= 1<<ledpin; // If something wrong, at least notify the user...
  }
}

// --- application ---


// The device loads a saved state at bootup and maybe later again.
// It saves a state when requested by the user.
// Currently a no-op because we don't actually have any state.

void save_state() {
}

void load_state() {
}

// The LED is sadly placed on SCK, so we don't use it right now.
void led_blink_event(void* param) {
  LEDs_ToggleLEDs(LEDS_LED1);

  int16_t i = (int16_t)param;

  if (i > 0) {
    i--;
    enqueue_event_rel(msec2ticks(250,TIMER_DIV),&led_blink_event,(void*)i);
  }
}

/*
//void EVENT_beep_done() {
void EVENT_melody_done() {
  // nothing for now.
}
*/

void EVENT_softosc_done(uint8_t channel) {
  if (channel == BEEP_CHANNEL)
    EVENT_beep_done();
}


// we use a melody structure so that we can make more complicated feedback sounds in the future. However we must keep track of the starting notes' indexes in the index array below.
melody_begin(feedback_melody,480)
  // 0, sleep (0)
  melody_note(c,-1,1)
  melody_note(b,-2,1)
  melody_note(a,-2,8)
  melody_stopmarker()
  // 4, bad (1)
  melody_note(a,-1,4)
  melody_stopmarker()
  // 6, good (2)
  melody_note(a,0,4)
  melody_stopmarker()
  // 8, start (3)
  melody_note(a,1,4)
  melody_stopmarker()
  // 10, end (4)
  melody_note(e,1,4)
  melody_stopmarker()
  // 12, wakeup (5)
  melody_note(a,2,4)
  melody_stopmarker()
  // 14, good pin (6)
  melody_note(a,0,2)
  melody_note(cis,1,2)
  melody_note(e,1,2)
  melody_note(a,1,3)
  melody_note(e,1,1)
  melody_note(a,1,4)
  melody_stopmarker()
  // 21, bad pin (7)
  melody_note(g,-1,3)
  melody_rest(1)
  melody_note(g,-1,3)
  melody_rest(1)
  melody_note(g,-1,8)
  melody_stopmarker()
  // 27, anything else (8)
  melody_note(a,0,8)
melody_end()

// sleep, bad, good, start, end, wakeup, good pin, bad pin, anything
static const uint8_t feedback_notes_ix[] PROGMEM = {0,4,6,8,10,12,14,21,27};
#define feedback_sounds_count (sizeof(feedback_notes_ix)/sizeof(feedback_notes_ix[0]))


void do_pinpad_feedback(int i) {
  i = i < feedback_sounds_count ? i : feedback_sounds_count-1;
  //melody_play(feedback_melody);
  melody_set_time_unit(melody_get_time_unit(&feedback_melody));
  uint8_t ix = pgm_read_byte(&feedback_notes_ix[i]);
  melody_play_notes(melody_get_notes(&feedback_melody)+ix);
}

#define inbuf_size 80
char inbuf[inbuf_size+1];
int inbuf_len = 0;

// This is ok:
//char* const inbuf_ptr PROGMEM = &inbuf[0];
#define pinpad_inbuf_size 32
char pinpad_inbuf[pinpad_inbuf_size+1];
int pinpad_inbuf_len = 0;

#define pinpad_timeout sec2ticks(10,TIMER_DIV)
bool pinpad_sleeping = false;

void pinpad_sleep_event(void* param) {
  pinpad_sleeping = true;
  pinpad_sleep();
  pinpad_inbuf_len = 0;
  do_pinpad_feedback(0);
  usart_msg("AWAKE=0\n");
}

void pinpad_be_used() {
  dequeue_events(&pinpad_sleep_event);
  enqueue_event_rel(pinpad_timeout,&pinpad_sleep_event,(void*)0);
}

void print_door_feedback(uint8_t mode, uint8_t success) {
  /*
    DOOR=$locked$closed${closing?1:opening?2}$succeeded\n
    (succeeded == fail?0:success?1:unfinished?2:-)
  */
  /*
  char msg[10] = "DOOR=....\n";
  msg[5] = door_is_locked()?'1':'0';
  msg[6] = door_is_closed()?'1':'0';
  msg[7] = mode+'0';
  msg[8] = success?'1':'0';
  usart_write(msg,10);
  */
  usart_msg("DOOR=");
  usart_writechar(door_is_locked()?'1':'0');
  usart_writechar(door_is_closed()?'1':'0');
  usart_writechar('0'+mode);
  usart_writechar('0'+success);
  usart_writechar('\n');
}

#define DOOR_REPORT_DELAY msec2ticks(200,TIMER_DIV)

void report_state_event(void* param) {
  print_door_feedback(door_mode, 2);
}

uint8_t recent_pins[3] = {0,0,0};
//int32_t last_keypress_time = 0;

void EVENT_Interrupt(uint8_t port, uint8_t pins) {
  // fill the entropy into the random buffer:
  uint32_t time = get_time();
  prng_write_byte(time & 0xff);

  uint8_t changedpins = pins ^ recent_pins[port];

  uint8_t door_imask = (1 << DOOR_SENSOR_PIN) | (1 << DOOR_BOLTSENSOR_PIN);

  if (port == 1) { // Port C
    if (pinpad_sleeping && (~pins & (1<<PINPAD_PIN))) {
      pinpad_sleeping = false;
      pinpad_unsleep();
      pinpad_be_used();
      do_pinpad_feedback(5);
      usart_msg("AWAKE=1\n");
    }
    if (changedpins & door_imask) {
      if (changedpins & (1 << DOOR_SENSOR_PIN))
        door_sensor_changed();
      if (changedpins & (1 << DOOR_BOLTSENSOR_PIN))
        door_boltsensor_changed();
      //print_door_feedback(0,success);
      dequeue_events(&report_state_event);
      enqueue_event_rel(DOOR_REPORT_DELAY,&report_state_event,NULL);
    }
  }
/*
  //  D6,D7,B0,B1: button ports, input (up,right,left,down)
  uint8_t mask = port == 0 ? 0x03 : port == 2 ? 0xc0 : 0;
  changedpins &= mask;
*/

  recent_pins[port] = pins;

#ifdef DEBUG_INTERRUPTS
  char msg[10] = "r0=......\n";
  msg[1] = '0'+port;
  inttohex(pins,&msg[3],2);
  msg[5] = '\n';
  msg[6] = 0;
  if (usart_writable_space() > 7) {
    usart_write(msg,6);
  }
#endif
}

ISR (PCINT0_vect, ISR_BLOCK)
{
  EVENT_Interrupt(0,PINB);
}

ISR (PCINT1_vect, ISR_BLOCK)
{
  EVENT_Interrupt(1,PINC);
}

ISR (PCINT2_vect, ISR_BLOCK)
{
  EVENT_Interrupt(2,PIND);
}

void setup_Interrupts(uint8_t maskB, uint8_t maskC, uint8_t maskD) {
  PCICR = ((maskB?1:0) << PCIE0)
        | ((maskC?1:0) << PCIE1)
        | ((maskD?1:0) << PCIE2);
  // PCIFR & (1<< PCIF0)
  PCMSK0 = maskB;
  PCMSK1 = maskC;
  PCMSK2 = maskD;
}
 
void buttons_init() {
  // enable pull-ups for B0,B1,D6,D7
  //PORTB |= 0x03;
  //PORTD |= 0xc0;
  
  uint8_t door_imask = (1 << DOOR_SENSOR_PIN) | (1 << DOOR_BOLTSENSOR_PIN);
  //setup_Interrupts(0x03,(1<<PINPAD_PIN) | door_imask,0xc0); // B0,B1,D6,D7
  setup_Interrupts(0x00,(1<<PINPAD_PIN) | door_imask,0x00);

  recent_pins[0] = PINB;
  recent_pins[1] = PINC;
  recent_pins[2] = PIND;
}

const char ok_msg[] PROGMEM = "OK.\n";
#define usart_ok() usart_write_P(ok_msg,sizeof(ok_msg)-1)

#define pinpad_debug_interval msec2ticks(100,TIMER_DIV)
void pinpad_debug_event(void* param) {
  int16_t value = adcw_state.values[PINPAD_PIN];
  char msg[5];
  snprintl(msg,4,value);
  msg[4] = 0;
#ifdef DEBUG_DISPLAY
  display_text(0,24,&testfont,msg);
#else
  usart_writechar('P');
  usart_write(msg,4);
  usart_writechar('\n');
#endif
  enqueue_event_rel(pinpad_debug_interval,&pinpad_debug_event,NULL);
}

void process_line() {
  char *s = inbuf;
  if (inbuf_len == 0)
    return;
  if (s[0] != '!') {
    // int len = inbuf_len;
    // we don't have any use for non-command data, so we just remind the user
    // to disable the tty's useless echo feature.
    usart_msg("!ECHO OFF\n");
  } else if (inbuf_len >= 2) {
    char cmd = s[1];
    char *param = &s[2];
    switch (cmd) {
      case '0': {
          // our protocol version. May also be used as a generic ping.
          usart_msg("VERSION 3\n");
        }
        break;
      case 'a': {
          // start/stop adnauseam playing.
          uint32_t do_play = hex2int(param);
          if (do_play) {
            adnauseam_play(korobeiniki_progression);
          } else {
            adnauseam_stop();
          }
        }
        break;
      case 'b': {
          // set a new baud rate. The user will have to adapt to get the OK.
          uint32_t baud = hex2int(param);
          usart_init_baud(baud);
          usart_ok();
        }
        break;
      case 'D': {
          // open/close the door.
          uint32_t open = hex2int(param);
          if (open)
            door_unlock();
          else
            door_lock();
          usart_ok();
        }
        break;
      case 'd': {
        // get current door state.
        dequeue_events(&report_state_event);
        enqueue_event_rel(DOOR_REPORT_DELAY,&report_state_event,NULL);
      }
      case 'f': {
          // blink the LED <param> times.
          LEDs_TurnOffLEDs(LEDS_LED1);
          uint16_t count = hex2int(param)*2-1;
          enqueue_event_rel(1,&led_blink_event,(void*)count);
        }
        break;
      case 'F': {
          // stop blinking the LED.
          // turn it on (param=1) or off (param=0) or leave it as it is (param=2)
          uint16_t stay = hex2int(param);
          dequeue_events(&led_blink_event);
          if (stay == 0) {
            LEDs_TurnOffLEDs(LEDS_LED1);
          } else if (stay == 1) {
            LEDs_TurnOnLEDs(LEDS_LED1);
          }
        }
        break;
      case 'G': {
          // read adc_watch channel <param>.
          uint8_t num = hex2int(param);
          usart_write(inbuf,inbuf_len);
          uint32_t value;
          value = adcw_state.values[num % 8];
          char msg[10];
          inttohex(value,msg,9);
          usart_writechar(' ');
          usart_write(msg,9);
          usart_writechar('\n');
        }
        break;
      case 'k':
        // play the korobeiniki main theme once.
        melody_play(&korobeiniki_a);
        break;
      case 'l':
        // currently a no-op.
        load_state();
        usart_ok();
        break;
      case 'm': {
          // play musical feedback sound <param>.
          uint32_t i = hex2int(param);
          do_pinpad_feedback(i);
        }
        break;
      case 'P': {
          // debug the pinpad by printing out adc readings.
          enqueue_event_rel(pinpad_debug_interval,&pinpad_debug_event,NULL);
          usart_ok();
        }
        break;
      case 's':
        // currently a no-op.
        save_state();
        usart_ok();
        break;
      case 't': {
          // beep with frequency <param> for half a second.
          uint16_t freq = hex2int(param);
          beep(freq,500);
        }
        break;
      case 'T': {
          // Get (up-)time. Use to verify that a reset has been done.
          uint32_t time = get_time();
          usart_msg("TIME=");
          char msg[9];
          inttohex(time,msg,8);
          usart_write(msg,8);
          usart_writechar('\n');
        }
        break;
      case 'z':
        // power down.
        poweroff();
        break;
    }
  }
}

void process_char(char c) {
  if (c == 10 || c == 13) {
    if (inbuf_len < inbuf_size) {
      inbuf[inbuf_len] = 0;
      process_line();
    }
    inbuf_len = 0;
  } else if (inbuf_len < inbuf_size) {
    if (c > 0x80 || c < 0x20) {
      inbuf_len = inbuf_size;
    } else {
      inbuf[inbuf_len] = c;
      inbuf_len++;
    }
  } // else ignore more characters and in the end the whole line.
}

void process_pinpad_line() {
  char *s = pinpad_inbuf;
  if (pinpad_inbuf_len == 0)
    return;

  // special single-digit codes (*X#).
  if (pinpad_inbuf_len == 1) {

    switch (pinpad_inbuf[0]) {
      // code to just lock the door: *0#
      case '0' :
        door_lock();
        //door_schedule_locking(sec2ticks(2,TIMER_DIV));
        return;
#ifdef ENABLE_EASTEREGGS
      // code to start music: *1#
      case '1' :
            adnauseam_play(korobeiniki_progression);
            return;
      // code to stop music: *2#
      case '2':
            adnauseam_stop();
            return;
#ifdef ENABLE_COPYRIGHTED_EASTEREGGS
      case '3':
            melody_play(&la_prapoutische);
            return;
      case '4':
            melody_play(&mccarthys_waltz);
            return;
#endif
#endif
      default:;
    }
  }
#ifdef DEBUG_BACKDOOR
  #warning(Backdoor Active)
  // TODO: remove this backdoor:
  if (strcmp(s,"12345") == 0) {
    door_unlock();
  }
#endif

#ifdef DEBUG_DISPLAY
  display_clear();
  display_text(0,8,&testfont,s);
#endif
  usart_msg("PIN=");
  usart_write(s,pinpad_inbuf_len);
  usart_msg("\n");
}

// TODO: beep accordingly
void process_pinpad_char(char c) {
  if (c == '#') {
    if (pinpad_inbuf_len < pinpad_inbuf_size) {
      pinpad_inbuf[pinpad_inbuf_len] = 0;
      process_pinpad_line();
    }
    pinpad_inbuf_len = 0;
    do_pinpad_feedback(4);
  } else if (c == '*') {
    pinpad_inbuf_len = 0;
    do_pinpad_feedback(3);
  } else if (pinpad_inbuf_len < pinpad_inbuf_size) {
    if (c > 0x80 || c < 0x20) {
      pinpad_inbuf_len = pinpad_inbuf_size;
    } else {
      pinpad_inbuf[pinpad_inbuf_len] = c;
      pinpad_inbuf_len++;
#ifdef DEBUG_DISPLAY
      if (pinpad_inbuf_len < pinpad_inbuf_size) {
        pinpad_inbuf[pinpad_inbuf_len] = 0;
        display_text(0,16,&testfont,pinpad_inbuf);
      }
#endif
      do_pinpad_feedback(2);
    }
  } // else ignore more characters and in the end the whole line.
}

void EVENT_USART_Read(char c) {
  // fill the entropy into the random buffer:
  prng_write_byte(get_time() & 0xff);

  process_char(c);
}

void EVENT_pinpad_keypressed(char c) {
  // fill the entropy into the random buffer:
  prng_write_byte(get_time() & 0xff);

  pinpad_be_used();
  if (c != 0)
    process_pinpad_char(c);
  else do_pinpad_feedback(1);
}

void EVENT_adc_watch(uint8_t channel, int16_t value) {
  if (channel == PINPAD_PIN) {
    pinpad_on_adc_read(value);
  } else if (channel == DOOR_MOTOR_SENSE_PIN) {
    door_on_motor_sense_read(value);
#ifdef DEBUG_MOTOR_SENSE
    char s[5];
    snprintl(s,4,value);
    s[4] = '\n';
    usart_msg("SENSE=");
    usart_write(s,5);
#endif
  } else {
    adc_watch_set_range(channel,value-2,value+2);
  }
}

/*
  1 = stall (before succeeding in whatever we're doing)
  2 = not running, but should
  3 = running, but should not
*/

void EVENT_door_motor_failing(uint8_t symptom) {
  //char msg[] = "MFAIL=0\n";
  //msg[6] = '0'+symptom;
  //usart_write(msg,8);
  usart_msg("MFAIL=");
  usart_writechar('0'+symptom);
  usart_writechar('\n');
}

void EVENT_door_locked(bool success) {
  do_pinpad_feedback(success?5:0);
  print_door_feedback(1,success?1:0);
}

void EVENT_door_unlocked(bool success) {
  do_pinpad_feedback(success?4:1);
  print_door_feedback(2,success?1:0);
}

void EVENT_door_mode_changed(uint8_t old_mode) {
  dequeue_events(&report_state_event);
  enqueue_event_rel(DOOR_REPORT_DELAY,&report_state_event,NULL);
}

/*
  gameboy project pins:
    D3,D4,D5: display ports, output (TODO: which ones?)
      There are: SS, D/C and reset...
    D6,D7,B0,B1: button ports, input (up,right,left,down)
                 (TODO: That's PCINT2 and PCINT0.)
    B3,B5: SDO,SCK for display
    3V3,GND for display, GND for buttons.

  project pins for fablock:
    pinpad: C4, A/D in/irq
    speaker: C5, out
    motor: C0,C1, out
    door sensor: C2, in/irq
    door bolt sensort: C3, in/irq
*/

void startup() {
  // disable watchdog if enabled
  wdt_reset();
  MCUSR &= ~(1 << WDRF);
  wdt_disable ();

  // disable any output, just in case
  DDRB = 0;
  DDRC = 0;
  DDRD = 0;
  // enable pullups
  PORTB = 0xff;
  PORTC = 0xff;
  PORTD = 0xff;

  // configure pins
  PORTB &= ~(1<<ledpin);
  DDRB |= 1<<ledpin;

  //adc_conf(true,0,7); // enable, no adc pins, slow prescaler
  // spec says 50..200kHz for accuracy, we have 16M for cpu, need factor 2^7.

  // setup event processing
  events_start(TIMER_SCALE);

  usart_init();
  buttons_init();
  door_init();

  adc_conf(true,0,ADC_DIV_128); // pins C3, ADC6 and ADC7
  adc_watch_init(0);
  pinpad_init();

/*
  NONATOMIC_BLOCK(NONATOMIC_FORCEOFF) {
    eepromfs_index(&eep_index,0,0);
  }
*/

#ifdef DEBUG_DISPLAY
  display_init();
#endif
  adc_watch_start();

  load_state();
}

int main() {

  startup();
  prng_seed(0);
  //application_start(0); // starts default application (menu).

  sei();


  // prepare for sleeping
  set_sleep_mode(SLEEP_MODE_IDLE);
   // _ADC, _PWR_DOWN, _PWR_SAVE, _STANDBY, _EXT_STANDBY
   // PWR_SAVE leaves Timer2 active, PWR_DOWN is rather off.
  while(true) {
    // let the ISRs handle the events.
    sleep_mode();
      /* or:
           sleep_enable(); sei(); sleep_cpu(); sleep_disable();
         for avoiding race conditions we don't have.
      */
  }
  return 0;
}
