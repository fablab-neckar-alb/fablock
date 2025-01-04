#include <limits.h>
#include <cstdint>
#include "pinpad_matrix.h"
#include "gtest/gtest.h"
namespace
{


//const int16_t pinpad_adc_values[12] PROGMEM =
//    {93, 171, 236, 293, 372, 411, 445, 476, 522, 545, 567, 586};
//const char pinpad_chars[12] PROGMEM =
//    {'*', '7', '4', '1', '0', '8', '5', '2', '#', '9', '6', '3'};

TEST(pinpad_matrix, testBorders)
{
    EXPECT_EQ('\0', get_pinpad_key(pinpad_min_dist-1));
    EXPECT_EQ('\0', get_pinpad_key(pinpad_max_valid));
}

TEST(pinpad_matrix, testValues)
{
  
  for(unsigned int i = 0; i < 12u; i++){
    
    int16_t value = pinpad_adc_values[i];
    char decoded = pinpad_chars[i];
    EXPECT_EQ(decoded, get_pinpad_key(value));
    
    for(int j = 0; j < pinpad_min_dist/2; j++){
      EXPECT_EQ(decoded, get_pinpad_key(value+j));
      EXPECT_EQ(decoded, get_pinpad_key(value-j));
    }
  }
    
}

}