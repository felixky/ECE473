/**********************************************************************
Author: Kyle Felix
Date: October 30th, 2019
Class: ECE 473 Microcontrollers
Descriptiion: In Lab 3, I will be implementing an LED display and a 
	pushbutton board on the ATmega128 using interrupts and SPI.
**********************************************************************/

//  HARDWARE SETUP:
//  PORTA is connected to the segments of the LED display. and to the pushbuttons.
//  PORTA.0 corresponds to segment a, PORTA.1 corresponds to segement b, etc.
//  PORTB bits 4-6 go to a,b,c inputs of the 74HC138.
//  PORTB bit 7 goes to the PWM transistor base.

//#define F_CPU 16000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

volatile int16_t sum;
volatile int8_t mode_sel;
volatile int8_t EC_a_prev;
volatile int8_t EC_b_prev;

void spi_init(){
   DDRB |= (1<<DDB0) | (1<<DDB1) | (1<<DDB2);	//output mode for SS, MOSI, SCLK 
   SPCR |= (1<<MSTR) | (1<<CPOL) | (1<<CPHA) | (1<<SPE);//master mode, clk low on idle,
// leading edge smaple , and spi enable 
   SPSR |= (1<<SPI2X);			//double speed operation  
}

uint8_t spi_read() {
   SPDR = 0x00;
   while(bit_is_clear(SPSR, SPIF)){}
   return SPDR;
}

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
   return;
}//segment_sum


int8_t bars() {
   for(int i = 0; i < 8; i++) {
      if(chk_buttons(i) == 1) {
   	 mode_sel = i;
      }
   }
   SPDR = mode_sel;
   while(bit_is_clear(SPSR, SPIF)){}
   PORTB |= (1<<PB0);
   PORTB &= 0xFE;
   
return mode_sel;
}

int8_t read_encoder() {
   uint8_t encoder_value;
   int8_t value = 0x00;
//   uint8_t ec_a[2];
//   uint8_t ec_b[2];
   uint8_t ec_a;
   uint8_t ec_b;

   //Shift_LD_N low
   PORTE &= 0x00;	//Begining of SHIFT_LD_N pulse. It is low here
   _delay_us(100);
   PORTE |= 0xFF;	//End of SHIFT_LD_N pulse. back to high
   PORTD &= 0x00;	//CLK_INH low

   encoder_value = spi_read();
   PORTD |= 0x02;	//CLK_INH high
   value = 1;//bars();
   ec_a = encoder_value & 0x03;  //Only grabs these bits 0000_0011
   ec_b = encoder_value & 0x0C;  //Only grabs these bits 0000_1100 

   if(ec_a != EC_a_prev){
      if(!(EC_a_prev) & (ec_a == 0x01)){
         value = value; //1;
      }
      else if(!(EC_a_prev) & (ec_a == 0x03)){
	 value = -(value); //-1;
      }
      else
	 value = 0;
   }
   else {//if(ec_b != EC_b_prev){
      if(!(EC_b_prev) & (ec_b == 0x01)){
         value = value; //1;
      }
      else if(!(EC_b_prev) & (ec_b == 0x02)){
	 value = -(value); //-1;
      }
      else
	 value = 0;
   }
EC_a_prev = ec_a;
EC_b_prev = ec_b;

 /*  ec_a[0] = encoder_value & 0x01;   
   ec_a[1] = encoder_value & 0x02;   
   ec_b[0] = encoder_value & 0x04;   
   ec_b[1] = encoder_value & 0x08;*/

  /* if(ec_a[0] != EC_a_prev) {   
      if(ec_a[0] > EC_a_prev){
	if(ec_a[0] == ec_a[1]){
    	   value = -1;
        }
	else
	   value = 1;
      }   
      else if(ec_a[0] < EC_a_prev){
   	if(ec_a[0] == ec_a[1]){
	   value = -1;
	}
	else
	   value = 1;
      }
      else
	value = 0;
   }
   else {
      if(ec_b[0] > EC_b_prev){
	if(ec_b[0] == ec_b[1]){
    	   value = -1;
        }
	else
	   value = 1;
      }   
      else if(ec_b[0] < EC_b_prev){
   	if(ec_b[0] == ec_b[1]){
	   value = -1;
	}
	else
	   value = 1;
      }
      else
	value = 0;
   }
EC_a_prev = ec_a[0];
EC_b_prev = ec_b[0];*/

return value;
}

ISR(TIMER0_OVF_vect) {
      sum = sum + read_encoder();
      if(sum>1023)
	sum = sum % 1023;
      if(sum<0)
	sum = 1023;

}

int main() {
   //uint16_t sum = 0x0000;

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
      bars();
      segsum(sum);			//Send sum to be formatted for the 7 seg display
      DDRA = 0xFF;			//Makes PORTA all outputs

      for( int j = 0; j < 5; j++) {	//cycles through each of the five digits
         PORTA = segment_data[j];	//Writes the segment data to PORTA aka the segments
         PORTB = j << 4;		//J is bound 0-4 and that value is shifted left 4 so that 
				//the digit to be displayed is in pin 4, 5, and 6 
         _delay_ms(1);		//delay so that the display does not flicker
      }
        PORTB = 0x00;
   }
return 0;
}




