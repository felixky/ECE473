// lm73_functions.c       
// Roger Traylor 11.28.10

#include <util/twi.h>
#include "lm73_functions.h"
#include <util/delay.h>

//TODO: remove volatile type modifier?  I think so.
//TODO: initalize with more resolution and disable the smb bus timeout
//TODO: write functions to change resolution, alarm etc.

volatile uint8_t lm73_wr_buf[2];
volatile uint8_t lm73_rd_buf[2];

//********************************************************************************

//******************************************************************************
uint8_t lm73_temp_convert(char temp_digits[], uint16_t lm73_temp, uint8_t f){
//given a temperature reading from an LM73, the address of a buffer
//array, and a format (deg F or C) it formats the temperature into ascii in 
//the buffer pointed to by the arguement.
//TODO:Returns what???(uint8_t)??? Probably a BUG?
   if(f){ //return in deg F

   }
   else{ //deg C

   }
//Yeah, this is for you to do! ;^)
return 0b11111111;
}//lm73_temp_convert
//******************************************************************************
