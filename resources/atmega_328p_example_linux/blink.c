#include <avr/io.h>
#include <util/delay.h>

int main()
{
    // Set built-in LED pin as output
    DDRD |= (1 << DDD2);
    while (1) {
        PORTD |=  (1 << PD2);   // LED on
        _delay_ms(500);
        PORTD &= ~(1 << PD2);   // LED off
        _delay_ms(500);
    }
    return 0;
}
