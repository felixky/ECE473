/**********************************************************************
Author: Kyle Felix
Date: October 30th, 2019
Class: ECE 473 Microcontrollers
Descriptiion: In Lab 4, I will be implementing an alarm clock on the 
	ATmega 128 with the use of an LCD, LED display, push button
	board, encoder, and bargraph. TNCT0 and 1 will be used through
	this project.
**********************************************************************/

/*			  HARDWARE SETUP:
-  PORTA is connected to the segments of the LED display. and to the pushbuttons.
-  PORTA.0 corresponds to segment a, PORTA.1 corresponds to segement b, etc.
-  PORTB bits 4-6 go to a,b,c inputs of the 74HC138.
-  PORTB bit 7 goes to the PWM transistor base.
-  PORTB bit 3 goes to SOUT on the encoder
-  PORTB bit 2 goes to SDIN for the Bargraph
-  PORTB but 1 goes to both SRCLK(Bargraph) and SCK(Encoder)
-  PORTB bit 0 goes to RegCLK on the Bargraph
-  PORTE bit 6 goes to SHIFT_LD_N on the encoder
-  PORTD bit 1 goes to CLK_INH on the encoder
-  PORTD bit 0 goes to S_IN on te encoder
-  PORTC bit 6 goes to OE_N on the bargraph  
**********************************************************************/
//#define F_CPU 16000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

volatile uint8_t hex = 0;
volatile uint16_t mult = 0;
volatile int16_t sum = 0;
volatile int16_t mode_sel = 1;
volatile int16_t prev_mode = 5;
volatile int8_t EC_a_prev;
volatile int8_t EC_b_prev;

/**********************************************************************
Function: spi_init() 
Description: Initialization of the serial port interface. More specifics
	in the function below. 
Parameters: NA
**********************************************************************/
void spi_init(){
   DDRB |= (1<<DDB0) | (1<<DDB1) | (1<<DDB2);	//output mode for SS, MOSI, SCLK 
   SPCR |= (1<<MSTR) | (1<<CPOL) | (1<<CPHA) | (1<<SPE);//master mode, clk low on idle,
// leading edge smaple , and spi enable 
   SPSR |= (1<<SPI2X);			//double speed operation  
}

/**********************************************************************
Function: spi_read()
Description: This function reads data from the SPI serially and returns 
	the 8 bit value that it read.
Parameters: NA
**********************************************************************/
uint8_t spi_read() {
   SPDR = 0x00;				//'Dummy' write to SPI
   while(bit_is_clear(SPSR, SPIF)){}	//Reads the 8 bits serially
   return SPDR;
}

/**********************************************************************
Function: segment_data[]
Description: This is an array that will hold the data that will be 
	displayed.
Parameters: NA
**********************************************************************/
//holds data to be sent to the segments. logic zero turns segment on
uint8_t segment_data[5] = {
}; 

/**********************************************************************
Function: dec_to_7seg[]
Description:decimal to 7-segment LED display encodings, logic "0" turns
	on segment
Parameters: NA
**********************************************************************/
// 0x(DP)(G)(F)(E)(D)(C)(B)(A), active low
uint8_t dec_to_7seg[18] = {
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
  0b10001000,   //A
  0b10000011,   //b
  0b11000110,	//C
  0b10100001,	//d
  0b10000110,	//E
  0b10001110,   //F
  0b11111111,	//All segments off
  0b00000000,	//All segments are on
 
};
/**********************************************************************
Function: chk_buttons                                      
Description: Checks the state of the button number passed to it. It 
	shifts in ones till the button is pushed. Function returns a 1 only 
	once per debounced button push so a debounce and toggle function can 
	be implemented at the same time. Adapted to check all buttons from 
	Ganssel's "Guide to Debouncing" Expects active low pushbuttons on PINA 
	port.  Debounce time is determined by external loop delay times 12. 
Parameters: A specific button number(0-7) to check if it is pressed
**********************************************************************/
uint8_t chk_buttons(uint8_t button) {
  static uint16_t state[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  state[button] = (state[button] << 1) | (! bit_is_clear(PINA,button))| 0xE000;
  if (state[button] == 0xF000)
    return 1;
return 0;
}

/***********************************************************************************
Function:segment_sum                                    
Description: takes a 16-bit binary input value and places the appropriate equivalent 
	4 digit BCD segment code in the array segment_data for display. array is 
	loaded at exit as:  |digit3|digit2|colon|digit1|digit0|
Parameters: A sum that willl be decoded
************************************************************************************/
void segsum(uint16_t sum) {
  int d0,d1,d2,d3; //,colon, digits;
  //determine how many digits there are 
  //break up decimal sum into 4 digit-segments
  //This block of code takes in the sum and finds the 0-9 value for each of the four led digits  
  if(hex) {
//Integer division and mod are used to convert decimal to hex
     d0 = ((sum%256)%16);//1s digit
     d1 = (sum %256)/16; //10s digit
     d2 = sum/256;	 //100s digit. Integer division
     d3 = 0;		 //1000s digit. 1023 will never need this digit

     segment_data[0] = dec_to_7seg[d0]; 
     segment_data[1] = dec_to_7seg[d1];
     segment_data[2] = 0xFF;
     segment_data[3] = dec_to_7seg[d2];
     segment_data[4] = 0xFF;
     
     if(sum < 0x100)	//Compares the sum to 255
	segment_data[3] = 0xFF;
     if(sum < 0x10)	//Compares the sum to 10
	segment_data[1] = 0xFF;
     
  }
  else {
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
  }
   return;
}//segment_sum


/**********************************************************************
Function: bar()
Description:This function reads in the button board and updates the 
	LEDs on the bargraph through the serial port.
Parameters: NA
**********************************************************************/
void bars() {
   DDRA = 0x00;				//Set all as inputs
   PORTA = 0xFF;			//Pull up resistors
   PORTB |= PINB | 0x70;		//Enable tristate buffer

   for(int i = 0; i < 8; i++) {		//Increment through all buttons
      if(chk_buttons(i) == 1) {		//Check if button is pressed
   	 mult = (1<<i);			//mult gets 2^i
      }
   }
   PORTB &= 0x00;
   if(mult == 128){			//Button 8 toggles base 10 and 16
      hex = !(hex);			//on the LED display
   }
   if(mult > 4) {			//I only want values from the
      mult = 0;				//first three buttons
   }
   //This switch statement is used to enable a 'toggle' functionality
   //so that modes can be selected an deselected
   switch(mult) {
      case 1:
	 if((mode_sel ^ mult) == 0){	//XOR to see if they are the same
	    mode_sel = 0;
	 }
	 else
	    mode_sel = 1;		//If not, then change mode
         break;
      case 2:
	 if((mode_sel ^ mult) && (mode_sel != 1)){//if cur &prev are diff
	    mode_sel = 0;//and prev isn't = 1 then two modes are selected
	 }
	 else
	    mode_sel = 2;		//If not, then change mode
         break;
      case 4:
	 if((mode_sel ^ mult) && (mode_sel != 1)){//if cur &prev are diff
	    mode_sel = 0;//and prev isn't = 1 then two modes are selected
	 }
	 else
	    mode_sel = 4;		//If not, then change mode
         break;
      default:
	 mode_sel = mode_sel;		//no/invalid button press
   }   
   mult = 0;				//clear mult for next pass

   DDRA = 0xFF;				//Set A to alloutputs
   SPDR = mode_sel;			//Write mode to the Bargraph
   while(bit_is_clear(SPSR, SPIF)){}	
   PORTB |= (1<<PB0);			//Rgclk high on bargraph
   PORTB &= 0xFE;			//Rgclk low on bargraph
   
return ;
}

/**********************************************************************
Function: read_encoder()
Description: THis function reads the SPI value of the encoders then uses
	a state machine to determine if the encoder is being turned cc
	or ccw. The value being return is + or - the mode_sel value.
Parameters: NA
**********************************************************************/
int8_t read_encoder() {
   uint8_t encoder_value;
   int8_t value = 0x00;
   uint8_t ec_a;
   uint8_t ec_b;

   //Shift_LD_N low
   PORTE &= 0x00;	//Begining of SHIFT_LD_N pulse. It is low here
   _delay_us(50);
   PORTE |= 0xFF;	//End of SHIFT_LD_N pulse. back to high
   PORTD &= 0x00;	//CLK_INH low

   encoder_value = spi_read();
   PORTD |= 0x02;	//CLK_INH high
   value = mode_sel;
   ec_a = encoder_value & 0x03;  //Only grabs these bits 0000_0011
   ec_b = encoder_value & 0x0C;  //Only grabs these bits 0000_1100 
   ec_b = (ec_b >> 2);

   if(ec_a != EC_a_prev){ //Compares curr encoder value to ast value 
      if(!(EC_a_prev) && (ec_a == 0x01)){//Determines CW rotation
         value = value;
      }
      else if(!(EC_a_prev) && (ec_a == 0x02)){//Determines CCW rotation
	 value = -(value); 
      }
      else	//If not one of the state changes above, do nothing
	 value = 0;
   }
   else {	//This is for encoder B
      if(!(EC_b_prev) && (ec_b == 0x01)){//CW Rotation
         value = value; 
      }
      else if(!(EC_b_prev) && (ec_b == 0x02)){//CCW Rotation
	 value = -(value);
      }
      else
	 value = 0;
   }
//Saves previous values into volatile variables
EC_a_prev = ec_a;
EC_b_prev = ec_b;

return value;
}

/**********************************************************************
Function: ISR(TIMER0_OVE_vect
Description: Timer 0 overflow compare match interrupt. I call bars to 
	update the mode and bargraph. Then I change the sum based on	
	the value returned from the encoder reading. Sum is then bounded
	from 0-1023. 1023+1=1 and 0-any number > 0 = 1023.
Parameters: NA
**********************************************************************/
ISR(TIMER0_OVF_vect) {
      bars();      
      sum = sum + read_encoder();
      if(sum>1023)
	sum = sum % 1023;
      if(sum<0)
	sum = 1023; //No overflow. If less than 0 always go to 1023.

}

/**********************************************************************
Function: main()
Description: Program interrupts are enabled, initial port declarations,
	and while loop are defined. The LED display is updated continuously 
	in the loop.
Parameters: NA
**********************************************************************/
int main() {
   TIMSK |= (1<<TOIE0);			//enable interrupts
   TCCR0 |= (1<<CS02) | (1<<CS00);	//normal mode, prescale by 128
 
   DDRC |= 0xFF; 
   DDRB |= 0xF0;				//PB4-6 is SEL0-2, PB7 is PWM
   DDRE |= 0x40;				//PE6 is SHIFT_LD_N
   DDRD |= 0x0B;				//PE1 is CLK_INH and PE2 is SRCLK
   PORTC |= 0x00;
   PORTD |= 0x02;
   PORTE |= 0xFF;

   spi_init();				//Initalize SPI

   sei();				//Enable interrupts
   
   while(1){
      segsum(sum);			//Send sum to be formatted for the 7 seg display

      for( int j = 0; j < 5; j++) {	//cycles through each of the five digits
         if(j == 2 && hex){
	    PORTA = 0b00000011;		//Turns on L3 on the LED display
	 }
	 else {
            if(hex){
	       PORTA = segment_data[j];
	    }
	    else {
               PORTA = segment_data[j];	//Writes the segment data to PORTA aka the segments
            }
         }
         PORTB = j << 4;		//J is bound 0-4 and that value is shifted left 4 so that 
				//the digit to be displayed is in pin 4, 5, and 6 
         _delay_us(200);		//delay so that the display does not flicker
      }
//        PORTB = 0x00;
   }
return 0;
}
