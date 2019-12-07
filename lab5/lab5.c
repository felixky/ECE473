/***********************************************************************
Author: Kyle Felix
Date: November 12, 2019
Class: ECE 473 Microcontrollers
Descriptiion: In Lab 4, I will be implementing an alarm clock on the 
	ATmega 128 with the use of an LCD, LED display, push button
	board, encoder, and bargraph. TNCT0-3 will be used through
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
-  PORTF bit 7 is used for the ADC input
-  PORTD bit 2 is used for the alarm frequency
-  PORTE bit 3 is used as the volume control 
**********************************************************************/
//#define F_CPU 16000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "hd44780.h"
#include "twi_master.h"
#include "lm73_functions.h"

char lcd_array[32] = "                                ";
volatile uint16_t l_temp;
char    lcd_string_array[16];  //holds a string to refresh the LCD
extern uint8_t lm73_rd_buf[2];
extern uint8_t lm73_wr_buf[2];
volatile uint8_t volume = 0x9F;
volatile uint8_t sec_count = 0;
volatile int8_t min_count = 0;
volatile int8_t hour_count = 0;
volatile uint8_t a_sec_count = 0;
volatile int8_t a_min_count = 0;
volatile int8_t a_hour_count = 0;
volatile uint8_t am_pm = 0;		//0=am 1=pm
volatile uint8_t mil = 1;		//military time is on by default
volatile uint8_t alarm = 0;
volatile uint8_t snooze = 0;

volatile uint8_t hex = 0;
volatile uint16_t mult = 0;
volatile int16_t sum = 0;
volatile int16_t mode_sel = 0;
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
Function: tcnt0_init
Description: Timer counter 0 is initialized in normal mode with no prescale
	and will be used as a seconds counter for the clock
Parameters: NA
**********************************************************************/
void tcnt0_init(){
   ASSR |= (1<<AS0);
   TIMSK |= (1<<OCIE0) | (1<<TOIE0);			//enable interrupts
   TCCR0 |= (1<<CS00);			//normal mode, no prescale
   OCR0 = 0xFF;
}

/**********************************************************************
Function: tcnt1_init
Description: Timer counter 1 is initialized in CTC mode, 64 bit prescaler
	so that it can be used to generate an alarm frequency
Parameters: NA
**********************************************************************/
void tcnt1_init(){
   TCCR1B |= (1<<CS11) | (1<<CS10) | (1<<WGM12);		//CTC mode, 64bit prescaler
   TCCR1C = 0x00;
   TIMSK  |= (1<<OCIE1A);	//enable flag for interrupt 
   OCR1A = 0x0040;		//compare match at 64
}

/**********************************************************************
Function: tcnt2_init
Description: This timer is used to dim the led display. It is in 
	normal mode with a 64 bit prescaler. OCRA2 is adjusted to change
	the pwn produced.
Parameters: NA
**********************************************************************/
void tcnt2_init(){
   TCCR2 |= (1<<WGM21) | (1<<WGM20)| (1<<CS20) | (1<<COM21);			
	//normal mode, 64-bit prescale
}

/**********************************************************************
Function: tcnt3_init
Description: This timer is used as a volume control for the audio amp.
	I adjust the value of OCR3A to change the pwm.
Parameters: NA
**********************************************************************/
void tcnt3_init(){
   TCCR3A |= (1<<COM3A1) | (1<<WGM31);	 //Clear on OCR3A match
   TCCR3B |= (1<<CS30) | (1<<WGM32) | (1<<WGM33);		//Fast PWM mode, no prescaler
   TCCR3C = 0x00;
   ICR3 = 0x9F;				//Setting the TOP value
   TCNT3 = 0x0000; 			//Initialize TNCT1 to 0
   OCR3A = 0x9F;			//Volume Control 0x9F=Max 0x00=Min
  
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
      alarm = !(alarm);			//on the LED display
   }
   if(mult == 64){		//Button 7 toggles snooze
      snooze = !(snooze);		//10 second snooze functionality
      a_sec_count = sec_count + 10;
      if(a_sec_count > 60){
         a_sec_count = a_sec_count % 60;
	 a_min_count++;
	 if(a_min_count == 60){
	    a_hour_count++;
	    a_min_count = 0;
	    if(a_hour_count == 24){
	       a_hour_count = 0;
	    }
	 }  
      }
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
	 if((mode_sel ^ mult) == 0 ){//if((mode_sel ^ mult) && (mode_sel == 4)){//if cur &prev are diff
	    mode_sel = 0;//and prev isn't = 1 then two modes are selected
	 }
	 else
	    mode_sel = 2;		//If not, then change mode
         
         break;
/*      case 4:
	 if((mode_sel ^ mult) && (mode_sel == 2)){//if cur &prev are diff
	    mode_sel = 0;//and prev isn't = 1 then two modes are selected
	 }
	 else
	    mode_sel = 4;		//If not, then change mode
         break;*/
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
   PORTD &= 0x0F;	//CLK_INH low

   encoder_value = spi_read();
   PORTD |= 0xF0;	//CLK_INH high
   value = mode_sel;
   ec_a = encoder_value & 0x03;  //Only grabs these bits 0000_0011
   ec_b = encoder_value & 0x0C;  //Only grabs these bits 0000_1100 
   ec_b = (ec_b >> 2);

//mode_sel == 0 means that no mode has been selected and the speaker volume can be adjusted
   if(mode_sel == 0){
      if(ec_a != EC_a_prev){ //Compares curr encoder value to ast value 
         if(!(EC_a_prev) && (ec_a == 0x01)){//Determines CW rotation
            volume += 10;	//increment volume
	    if(volume <= 0x9F){
		OCR3A = volume;	//maximum volume
	    }
	    else {
		volume = 0x9F;
		OCR3A = 0x9F;
	    }
         }
         else if(!(EC_a_prev) && (ec_a == 0x02)){//Determines CCW rotation
	    volume -= 10;	//decrement volume 
	    if(volume >= 0x00){
		OCR3A = volume;	//minimum volume
	    }
	    else {
		volume = 0x00;
		OCR3A = 0x00;
	    }
         }
         else	//If not one of the state changes above, do nothing
	 volume = volume;
      }
/*      else {	//This is for encoder B
         if(!(EC_b_prev) && (ec_b == 0x01)){//CW Rotation
            min_count = min_count + 1;//value = value;
	    if(min_count == 60){
	       min_count = 0; 
	       hour_count++;
	       if(hour_count > 23)
	          hour_count = 0;
	    }
         }
         else if(!(EC_b_prev) && (ec_b == 0x02)){//CCW Rotation
	    min_count = min_count - 1; //value = -(value);
	    if(min_count < 0){
	       min_count = 59;
	       hour_count--;
	       if(hour_count < 0){
	          hour_count = 23;
	       }
	    }
         }
         else
	    value = 0;
      }*/
   }

//mode_sel == 1 means that the user has selected the "time change" mode
   if(mode_sel == 1){
      if(ec_a != EC_a_prev){ //Compares curr encoder value to ast value 
         if(!(EC_a_prev) && (ec_a == 0x01)){//Determines CW rotation
            hour_count = hour_count + 1;//value = value;
	    if(hour_count == 24)
	       hour_count = 0;
         }
         else if(!(EC_a_prev) && (ec_a == 0x02)){//Determines CCW rotation
	    hour_count = hour_count - 1;//value = -(value);
	    if(hour_count < 0)
	       hour_count = 23; 
         }
         else	//If not one of the state changes above, do nothing
	 value = 0;
      }
      else {	//This is for encoder B
         if(!(EC_b_prev) && (ec_b == 0x01)){//CW Rotation
            min_count = min_count + 1;//value = value;
	    if(min_count == 60){
	       min_count = 0; 
	       hour_count++;
	       if(hour_count > 23)
	          hour_count = 0;
	    }
         }
         else if(!(EC_b_prev) && (ec_b == 0x02)){//CCW Rotation
	    min_count = min_count - 1; //value = -(value);
	    if(min_count < 0){
	       min_count = 59;
	       hour_count--;
	       if(hour_count < 0){
	          hour_count = 23;
	       }
	    }
         }
         else
	    value = 0;
      }
   }
//mode_sel == 2 means that the user has selected the "alarm time change" mode
   if(mode_sel == 2){
      if(ec_a != EC_a_prev){ //Compares curr encoder value to ast value 
         if(!(EC_a_prev) && (ec_a == 0x01)){//Determines CW rotation
            a_hour_count = a_hour_count + 1;//value = value;
	    if(a_hour_count == 24)
	       a_hour_count = 0;
         }
         else if(!(EC_a_prev) && (ec_a == 0x02)){//Determines CCW rotation
	    a_hour_count = a_hour_count - 1;//value = -(value);
	    if(a_hour_count < 0)
	       a_hour_count = 23; 
         }
         else	//If not one of the state changes above, do nothing
	 value = 0;
      }
      else {	//This is for encoder B
         if(!(EC_b_prev) && (ec_b == 0x01)){//CW Rotation
            a_min_count = a_min_count + 1;//value = value;
	    if(a_min_count == 60){
	       a_min_count = 0; 
	       a_hour_count++;
	       if(a_hour_count > 23)
	          a_hour_count = 0;
	    }
         }
         else if(!(EC_b_prev) && (ec_b == 0x02)){//CCW Rotation
	    a_min_count = a_min_count - 1; //value = -(value);
	    if(a_min_count < 0){
	       a_min_count = 59;
	       a_hour_count--;
	       if(a_hour_count < 0){
	          a_hour_count = 23;
	       }
	    }
         }
         else
	    value = 0;
      }
   }
//Saves previous values into volatile variables
EC_a_prev = ec_a;
EC_b_prev = ec_b;

return value;
}

/**********************************************************************
Function: get_local_temp()
Description: 
Parameters: NA
**********************************************************************/
void get_local_temp(){
uint16_t lm73_temp;

  //_delay_ms(65); //tenth second wait
  clear_display();                  //wipe the display
  twi_start_rd(LM73_ADDRESS, lm73_rd_buf, 2); //read temperature data from LM73 (2 bytes) 
  _delay_ms(1);    //wait for it to finish
  lm73_temp = lm73_rd_buf[0]; //save high temperature byte into lm73_temp
  lm73_temp = lm73_temp << 8; //shift it into upper byte 
  lm73_temp |= lm73_rd_buf[1]; //"OR" in the low temp byte to lm73_temp 
  itoa(lm73_temp >> 7, lcd_string_array, 10); //convert to string in array with itoa() from avr-libc                           

  lcd_array[16] = 'L';
  lcd_array[17] = ':';  
  lcd_array[18] = lcd_string_array[0];  
  lcd_array[19] = lcd_string_array[1];  
  lcd_array[20] = 'C';  
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
   static uint8_t count_7_8125ms = 0;

   count_7_8125ms++;
   if((count_7_8125ms % 128) == 0) { //interrupts every 1 second
      sec_count++;
   }
   bars();  
   read_encoder();      

}

ISR(TIMER0_COMP_vect) {
   static uint8_t count7_8125ms = 0;

   count7_8125ms++;
   if((count7_8125ms % 128) == 0) { //interrupts every 1 second
      get_local_temp();
   }

}

/**********************************************************************
Function: clock_time
Description: This function is used to change the time of the clock.The
	seconds count is incremented and once it reaches 60 mins are 
	incremented and so on. Depending on the mode that the user 
	selected alarm time or normal time is displayed.
Parameters: NA
**********************************************************************/
void clock_time(){ //by default we use military time
//This block is used to increment the time based on an increasing seconds count   
   if(sec_count == 60){
      min_count++;
      sec_count = 0;
      if(min_count == 60){
	 hour_count++;
	 min_count = 0;
	 if(hour_count == 24){
	    hour_count = 0;
	 }//hours	
      }//mins
   }//secs

//This is where the digits are written to the data array
      if(mode_sel == 2){	//display alarm time
         segment_data[4] = dec_to_7seg[a_hour_count/10];
         segment_data[3] = dec_to_7seg[a_hour_count%10];
         if(sec_count%2){segment_data[2] = 0b100;}		//Turn colon on
         else {segment_data[2] = 0b111;}		//Turn colon off
         segment_data[1] = dec_to_7seg[a_min_count/10];
         segment_data[0] = dec_to_7seg[a_min_count%10];
      }
      else{			//display military time
         segment_data[4] = dec_to_7seg[hour_count/10];
         segment_data[3] = dec_to_7seg[hour_count%10];
         if(sec_count%2){segment_data[2] = 0b100;}		//Turn colon on
         else {segment_data[2] = 0b111;}		//Turn colon off
         segment_data[1] = dec_to_7seg[min_count/10];
         segment_data[0] = dec_to_7seg[min_count%10];
      }
}

/**********************************************************************
Function: port_init
Description: General port initialization and setting pull up resistors
Parameters: NA
**********************************************************************/
void port_init(){
   DDRC |= 0xFF; 
   DDRB |= 0xF0;				//PB4-6 is SEL0-2, PB7 is PWM
   DDRE |= 0x4F;				//PE6 is SHIFT_LD_N
   DDRD |= 0xFF;				//PE1 is CLK_INH and PE2 is SRCLK
   PORTC |= 0x01;
   PORTD |= 0xFF;
   PORTE |= 0xFF;
}

/**********************************************************************
Function: change_alarm_state
Description: This function is used to display when the alarm is armed
	and what time it is set for on the LCD.
Parameters: NA
**********************************************************************/
void change_alarm_state(){
   static uint8_t curr = 0;
/*   if(alarm && (curr == 0)){	//First time through function with alarm on
      if((a_hour_count > 9) && (a_min_count > 9)){	//hours and min counts > 9
         string2lcd("ALARM at ");
         lcd_int16(a_hour_count, 2, 0, 0, 0);
         string2lcd(":");
         lcd_int16(a_min_count, 2, 0, 0, 0);
      }
      else if(a_hour_count > 9){ //hour>9 and min<10
         string2lcd("ALARM at ");
         lcd_int16(a_hour_count, 2, 0, 0, 0);
         string2lcd(":");
         string2lcd("0");
         lcd_int16(a_min_count, 1, 0, 0, 0);
      }
      else if(a_min_count > 9){ //min>9 and hour<10
         string2lcd("ALARM at");
         lcd_int16(a_hour_count, 2, 0, 0, 0);
         string2lcd(":");
         lcd_int16(a_min_count, 2, 0, 0, 0);
      }
      else { //hour and min < 10
         string2lcd("ALARM at");
         lcd_int16(a_hour_count, 2, 0, 0, 0);
         string2lcd(":");
         string2lcd("0");
         lcd_int16(a_min_count, 1, 0, 0, 0);
      }
      curr = 1;	//indicated that alarm has been written to the display next time
	//through the function
   }
	//clears display when the alarm is turned off*/
   if(alarm && (curr ==0)){
      //string2lcd("Alarm");
      lcd_array[0] = 'A';
      lcd_array[1] = 'l';
      lcd_array[2] = 'a';
      lcd_array[3] = 'r';
      lcd_array[4] = 'm';
      curr = 1;
   }
   else if((!alarm) && (curr == 1)){
      curr = 0;
      lcd_array[0] = ' ';
      lcd_array[1] = ' ';
      lcd_array[2] = ' ';
      lcd_array[3] = ' ';
      lcd_array[4] = ' ';
      //clear_display();
   }
   else{}
}

/**********************************************************************
Function: adc_init
Description: Basic adc initialization used for single shot adc readings
Parameters: NA
**********************************************************************/
void adc_init(){
//Initalize ADC and its ports
   DDRF  &= ~(_BV(DDF7)); //make port F bit 7 is ADC input  
   PORTF &= ~(_BV(PF7));  //port F bit 7 pullups must be off
   ADMUX |= (1<<REFS0) | (1<<MUX2)| (1<<MUX1)| (1<<MUX0); //single-ended, input PORTF bit 7, right adjusted, 10 bits

   ADCSRA |= (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);//ADC enabled, don't start yet, single shot mode 
                             //division factor is 128 (125khz)
}

/**********************************************************************
Function: fetch_adc
Description: This function reads the adc and adjust the value to change
	the pwm on tcnt2.
Parameters:NA
**********************************************************************/
void fetch_adc(){
   uint16_t adc_result;
   uint16_t step;   
   uint16_t step2;   

   ADCSRA |= (1<<ADSC); //poke ADSC and start conversion
   while(bit_is_clear(ADCSRA, ADIF)){} //spin while interrupt flag not set
   ACSR |= (1<<ACI); //its done, clear flag by writing a one 
   adc_result = ADC;                      //read the ADC output as 16 bits

   step = adc_result/4;//scales the adc result from 0-255
   step2 =  255 - step;//I need the complement to the adc result
   if(step2 > 235){	//this is a minimum brightness level
      step2 = 235;
   }

   OCR2 = step2;	//Write brightness level to tnct2 compare match register
	//to chaange the duty cycle
}

/**********************************************************************
Function: TIMER!_COMPA_vect
Description: This ISR creates the alarm frequency on PORTD but 3 the is used
	for the alarm tone.
Parameters: NA
**********************************************************************/
ISR(TIMER1_COMPA_vect){
   //static uint8_t seconds = 0;
   //bars();  //displays the mode on the bargraph
   //read_encoder();     //reads the encoder values
if(!snooze){ //alarm has not been snoozed
//checks to see alarm is on and the alarm time matches clock time
   if(alarm && ((hour_count == a_hour_count) && (min_count == a_min_count))){
      PORTD = PIND ^ 0b00000100;//toggles PD3 to create frequency
   }
}
}
/**********************************************************************
Function: snoozin
Description: This function disables the snooze variable once the alarm
	seconds equals the normal seconds. 
Parameters: NA
**********************************************************************/
void snoozin() {
   if(snooze){
      if(a_sec_count == sec_count)
         snooze = 0;
      else 
         snooze = 1;
   }
}

/**********************************************************************
Function: get_local_temp()
Description: 
Parameters: NA
**********************************************************************/
void local_temp_init(){

lm73_wr_buf[0] = 0x00; //load lm73_wr_buf[0] with temperature pointer address
twi_start_wr(LM73_ADDRESS, lm73_rd_buf, 1); //start the TWI write process
_delay_ms(2);    //wait for the xfer to finish

}


/**********************************************************************
Function: ()
Description: 
Parameters: NA
**********************************************************************/

/**********************************************************************
Function: main()
Description: Program interrupts are enabled, initial port declarations,
	and while loop are defined. The LED display is updated continuously 
	in the loop.
Parameters: NA
**********************************************************************/
int main() {
   spi_init();				//Initalize spi, counters,adc, and lcd
   tcnt0_init();
   tcnt1_init();
   tcnt2_init();
   tcnt3_init();
   port_init();
   adc_init();
   init_twi();
   local_temp_init();   

   lcd_init();
   sei();				//Enable interrupts
   while(1){
      refresh_lcd(lcd_array);
      snoozin();
      fetch_adc();
      clock_time();
      change_alarm_state();
      for( int j = 0; j < 5; j++) {	//cycles through each of the five digits
         if(alarm){
	    segment_data[2] &= 0b011;
	 }
	 PORTA = segment_data[j];	//Writes the segment data to PORTA aka the segments
         PORTB = j << 4;		//J is bound 0-4 and that value is shifted left 4 so that 
				//the digit to be displayed is in pin 4, 5, and 6 
         _delay_us(200);		//delay so that the display does not flicker
         PORTA = 0xFF;
      }
	PORTB = 0x00;;
   }
return 0;
}
