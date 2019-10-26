#define F_CPU 16000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#include <avr/io.h>
#include <util/delay.h>


uint8_t main() {

   DDRE |= 0x40;
   DDRD |= 0x06;
   DDRB |= 0xF0;




return 0;
}
