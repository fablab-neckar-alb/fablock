#ifndef CONFIG_COMMON_H
#define CONFIG_COMMON_H
#define BEEP_CHANNEL 0  // TIMER? 


#define COMMON_PORT C   // PORT where most stuff is connected(ADC PORT)
// USED PINS(2,3,4,5)
#define DOOR_SENSOR_PORT COMMON_PORT // Sensor on top of the door (open closed)
#define DOOR_SENSOR_PIN 2
#define DOOR_BOLTSENSOR_PORT COMMON_PORT // Sensor inside the lock cylinder(locked/unlocked)
#define DOOR_BOLTSENSOR_PIN 3


#define PINPAD_PORT COMMON_PORT
#define PINPAD_PIN 4    // ARDUINO PIN 
#define BEEP_PORT COMMON_PORT
#define BEEP_PIN 5      // ARDUINO PIN
#define MOTOR_SENSOR_PORT COMMON_PORT
#define MOTOR_SENSOR_PIN 7
#endif /* CONFIG_COMMON_H */
