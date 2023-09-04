#define F_CPU 16000000
#include <avr/io.h>
#include <util/delay.h>

#define blink_delay 1000

int main()
{
  DDRB = _BV(PORTB2); // pin 16
  while(1)
  {
    PORTB |= _BV(PORTB2);
    _delay_ms(blink_delay);
    PORTB &= ~_BV(PORTB2);
    _delay_ms(blink_delay);
  }
}
