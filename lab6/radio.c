//Radio test code

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <math.h>
#include <stdlib.h>
#include "twi_master.h"
#include "uart_functions.h"
#include "si4734.h"

extern enum radio_band{FM, AM, SW};
extern volatile uint8_t STC_interrupt;

volatile enum radio_band current_radio_band = FM;

uint16_t eeprom_fm_freq;
uint16_t eeprom_am_freq;
uint16_t eeprom_sw_freq;
uint8_t  eeprom_volume;

volatile uint16_t current_fm_freq = 9990;
volatile uint16_t current_am_freq;
volatile uint16_t current_sw_freq;
uint8_t  current_volume;

//Used in debug mode for UART1
//char uart1_tx_buf[40];      //holds string to send to crt
//char uart1_rx_buf[40];      //holds string that recieves data from uart


//******************************************************************************
// External interrupt 7 is on Port E bit 7. The interrupt is triggered on the
// rising edge of Port E bit 7.  The i/o clock must be running to detect the
// edge (not asynchronouslly triggered)
//******************************************************************************
ISR(INT7_vect){
	STC_interrupt = TRUE;
	PORTF ^= (1 << PF1);
}
/***********************************************************************/


int main(){
        init_twi();

	DDRF |= (1 << PF1);
	PORTF |= (0 << PF1);

        //Setup audio output (max)
	DDRE  |= (1 << PE3);
        PORTE |= (1 << PE3);
	


        EICRB |= (1<<ISC71) | (1<ISC70);
	EIMSK |= (1<<INT7);



	_delay_ms(1000);



	sei();

	
	   while(1){

		DDRE  |= (1 << PE2); //Port E bit 2 is active high reset for radio 
		PORTE |= (1 << PE2); //radio reset is on at powerup (active high)

		//hardware reset of Si4734
		PORTE &= ~(1<<PE7); //int2 initially low to sense TWI mode
		DDRE  |= 0x80;      //turn on Port E bit 7 to drive it low
		PORTE |=  (1<<PE2); //hardware reset Si4734 
		_delay_us(200);     //hold for 200us, 100us by spec         
		PORTE &= ~(1<<PE2); //release reset 
		_delay_us(30);      //5us required because of my slow I2C translators I suspect
		//Si code in "low" has 30us delay...no explaination
		DDRE  &= ~(0x80);   //now Port E bit 7 becomes input from the radio interrupt

		fm_pwr_up(); //powerup the radio as appropriate
		current_fm_freq = 9910; //arg2, arg3: 99.9Mhz, 200khz steps
		fm_tune_freq(); //tune radio to frequency in current_fm_freq

		_delay_ms(10000);


		

	   }
	 
}
