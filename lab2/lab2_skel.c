// lab2_skel.c 
// R. Traylor
// 9.12.08
// Modified by K. Felix on 10.10.19 

//  HARDWARE SETUP:
//  PORTA is connected to the segments of the LED display. and to the pushbuttons.
//  PORTA.0 corresponds to segment a, PORTA.1 corresponds to segement b, etc.
//  PORTB bits 4-6 go to a,b,c inputs of the 74HC138.
//  PORTB bit 7 goes to the PWM transistor base.

#define F_CPU 16000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#include <avr/io.h>
#include <util/delay.h>

//holds data to be sent to the segments. logic zero turns segment on
uint8_t segment_data[5] = {
}; 

//decimal to 7-segment LED display encodings, logic "0" turns on segment
// 0x(DP)(G)(F)(E)(D)(C)(B)(A), active low
uint8_t dec_to_7seg[12] = {
  0b11000000, 	//0
  0b11111001,	//1	
  0b10100100,	//2
  0b10110000,	//3
  0b10011001,	//4
  0b10010010,	//5
  0b10000010,	//6
  0b11111000,	//7
  0b10000000,	//8
  0b10010000,	//9
  0b11111111,	//All segments off
  0b00000000,	//All segments are on
 
};


//******************************************************************************
//                            chk_buttons                                      
//Checks the state of the button number passed to it. It shifts in ones till   
//the button is pushed. Function returns a 1 only once per debounced button    
//push so a debounce and toggle function can be implemented at the same time.  
//Adapted to check all buttons from Ganssel's "Guide to Debouncing"            
//Expects active low pushbuttons on PINA port.  Debounce time is determined by 
//external loop delay times 12. 
//
uint8_t chk_buttons(uint8_t button) {
  static uint16_t state[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  state[button] = (state[button] << 1) | (! bit_is_clear(PINA,button))| 0xE000;
  if (state[button] == 0xF000)
    return 1;
return 0;
}
//******************************************************************************

//***********************************************************************************
//                                   segment_sum                                    
//takes a 16-bit binary input value and places the appropriate equivalent 4 digit 
//BCD segment code in the array segment_data for display.                       
//array is loaded at exit as:  |digit3|digit2|colon|digit1|digit0|
void segsum(uint16_t sum) {
  int d0,d1,d2,d3; //,colon, digits;
  //determine how many digits there are 
  //break up decimal sum into 4 digit-segments
  //This block of code takes in the sum and finds the 0-9 value for each of the four led digits  
  d0 = (sum % 10);		//1's digit
  d1 = (((sum % 100) / 10) % 10);	//10's digit
  d2 = (sum / 100) % 10;		//100's digit
  d3 = (sum / 1000) % 10;		//1000's digit

  //This block changes the decimal from just above into 8-bits that can be displayed on the segments 
  segment_data[0] = dec_to_7seg[d0]; 
  segment_data[1] = dec_to_7seg[d1];
  segment_data[2] = 0xFF;
  segment_data[3] = dec_to_7seg[d2];
  segment_data[4] = dec_to_7seg[d3];

  //blank out leading zero digits and determine number of digits
    if(sum < 0x3E8)	//Compares the sum to 1000
	segment_data[4] = 0xFF;
    if(sum < 0x64)	//Compares the sum to 100
	segment_data[3] = 0xFF;
    if(sum < 0xA)	//Compares the sum to 10
	segment_data[1] = 0xFF;
//    if(sum == 0x0)	//Compares the sum to 0
//	segment_data[0] = 0xFF;
  //now move data to right place for misplaced colon position
   return;
}//segment_sum
//***********************************************************************************


//***********************************************************************************
uint8_t main() {
  uint16_t sum = 0;
//  int count = 0;

  DDRB = 0xF0;		//setting port B pins 4-7 as outputs
			//Setting a DDRx pin high makes it an output
while(1){
  _delay_ms(2);		//insert loop delay for debounce


  DDRA = 0x00;		//Makes PORTA all inputs
  PORTA = 0xFF;		//Sets pullups resistors

  PORTB = PINB | 0x70;	//Takes the current PINB and ORs it so that 
			//pin 4, 5, and 6 are high. 0x70 = 0b0111_0000
			//thus enabling the tristate buffer

  for(int i = 0; i < 8; i++) {	//increments through buttons 0-7
    if(chk_buttons(i) == 1) {	//Checks if each button is pressed 
	sum = sum + (1 << i);	//equivalent to sum = sum + 2^n where
				//n is the number of the button that was pressed
    }
  }

  PORTB = 0x00;			//We are only using pin 4-7 on PORTB and this action 
			//keeps the pwm pin low as well as disabling the tristate buffer
  if(sum > 1023) {
    sum = sum % 1023;		//bounds the sum of the buttons 0-1023
  }
  //break up the disp_value to 4, BCD digits in the array: call (segsum)
  segsum(sum);

  DDRA = 0xFF;			//Makes PORTA all outputs

  for( int j = 0; j < 5; j++) {	//cycles through each of the five digits
    PORTA = segment_data[j];	//Writes the segment data to PORTA aka the segments
    PORTB = j << 4;		//J is bound 0-4 and that value is shifted left 4 so that 
				//the digit to be displayed is in pin 4, 5, and 6 
    _delay_ms(1);		//delay so that the display does not flicker
  }

  }//while
return 0;
}//main
