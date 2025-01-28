#ifndef MOTOR_INTERFACE_H
#define MOTOR_INTERFACE_H
#include <stdint.h>


#define DOOR_MOTOR_DIR_STOP 0
#define DOOR_MOTOR_DIR_LOCK 1
#define DOOR_MOTOR_DIR_UNLOCK 2
/*
  0 = stop
  1 = forward
  2 = backward
*/
void door_set_motor(uint8_t dir);
void door_motor_init();

#endif /* MOTOR_INTERFACE_H */
