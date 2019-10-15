// lab1_code.c 
// created by: R. Traylor
// edited by: Kyle Feliz 9/30/19
// 7.21.08

//This program increments a binary display of the number of button pushes on switch 
//S0 on the mega128 board.

#include <avr/io.h>
#include <util/delay.h>

//*******************************************************************************
//                            debounce_switch                                  
// Adapted from Ganssel's "Guide to Debouncing"            
// Checks the state of pushbutton S1 It shifts in ones till the button is pushed. 
// Function returns a 1 only once per debounced button push so a debounce and toggle 
// function can be implemented at the same time.  Expects active low pushbutton on 
// Port D bit zero.  Debounce time is determined by external loop delay times 12. 
//*******************************************************************************
int8_t debounce_switch() {
  static uint16_t state = 0; //holds present state
  state = (state << 1) | (! bit_is_clear(PIND, 0)) | 0xE000;
  if (state == 0xF000) return 1;
  return 0;
}

//*******************************************************************************
// Check switch S0.  When found low for 12 passes of "debounce_switch(), increment
// PORTB.  This will make an incrementing count on the port B LEDS. 
//*******************************************************************************
int main() {
DDRB = 0xFF;  //set port B to all outputs
int lower, higher;
lower = 0;
higher = 0;

while(1){     //do forever
 if(debounce_switch()) {
    if(PINB == 0b10011001){	//condition to see if 99 is displayed on PORTB 
	    lower = 0;		//set couters to zero
	    higher = 0;
	    PORTB = 0X00;	//clear PORTB and display all off LEDs
    }
    else{
	    if(lower == 9){	//lower digit counter 0-9
		    lower = 0;	
		    higher++;	//once lower reaches 9, higher needs to be incremented
	    }
	    else{		//when lower is not 9
		    lower++;	//increment lower
	    }
	    PORTB = (higher*16) + lower;	//Shift higher 4 bits and adds lower to that value 
    }
 	 //PORTB++;
 }  //if switch true for 12 passes, increment port B
  _delay_ms(2);                    //keep in loop to debounce 24ms
  } //while 
} //main
