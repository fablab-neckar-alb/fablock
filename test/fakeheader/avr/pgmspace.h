#ifndef __PGMSPACE_H_
#define __PGMSPACE_H_ 1

#define PROGMEM
short pgm_read_word(const short* address){
  return *address;
}

char pgm_read_byte(const char* address){
  return *address;
}

#endif