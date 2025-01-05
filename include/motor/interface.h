#ifndef MOTOR_INTERFACE_H
#define MOTOR_INTERFACE_H
#include <stdint.h>

/*
  0 = stop
  1 = forward
  2 = backward
*/
void door_set_motor(uint8_t value);
void door_motor_init();

#endif /* MOTOR_INTERFACE_H */
