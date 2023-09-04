#include "uart.h"

#include<xc.h>
#include<stdbool.h>
#include<string.h>

typedef enum {
    START_BIT,
    DATA_BIT,
    STOP_BIT,
    ITERATOR_EOF
} IteratorState;

typedef struct {
    unsigned char *data;
    unsigned int length;
    unsigned int index;
    unsigned char data_bit_index;
    IteratorState state;
} BitIterator;

typedef struct {
    volatile bool in_use;
    BitIterator bit_iterator;
} Uart;

Uart uart;

void bit_iterator_init(BitIterator *bit_iterator, unsigned char *data, int length) {
    bit_iterator->data = data;
    bit_iterator->length = (length < 0) ? strlen((char *)data) : (unsigned int)length;
    bit_iterator->index = 0;
    bit_iterator->state = START_BIT;
    bit_iterator->data_bit_index = 0;
}

void uart_start() {
    uart.in_use = true;
    TMR2 = 0;
    TMR2ON = 1; // Start timer
}

void uart_stop() {
    TMR2ON = 0; // Stop timer
    TMR2 = 0; // TMR2 is not reset when writing TMR2ON.
    uart.in_use = false;
}

void uart_init(void) {
    // Setting RA2 to output and hold line
    TRISA2 = 0;
    RA2 = 1;

    PR2 = 104; // baud 9600
    TMR2IE = 1; // Enable timer2 interrupt
    TMR2IF = 0; // Clear timer2 interrupt event flag
    TOUTPS0 = 1; // Pre-scaler to 1:2 -> 1us per increment

    PEIE = 1;
    GIE = 1;

    bit_iterator_init(&uart.bit_iterator, NULL, 0);
    uart_stop();
}

void uart_write(unsigned char *data, int length) {
    bit_iterator_init(&uart.bit_iterator, data, length);

    if (!uart.bit_iterator.length)
        return;

    uart_start();

    while(uart.in_use);
}

typedef enum {
    STREAM_ZERO = 0,
    STREAM_ONE = 1,
    STREAM_EOF = 2
} StreamRetVal;

inline bool last_data_bit_p(BitIterator *iterator) {
    return iterator->data_bit_index == 8;
}

inline bool end_of_data(BitIterator *iterator) {
    return iterator->length == iterator->index;
}

StreamRetVal
next(BitIterator *iterator) {
    switch (iterator->state)
    {
        case ITERATOR_EOF:
            return STREAM_EOF;

        case START_BIT:
            iterator->state = DATA_BIT;
            return STREAM_ZERO;

        case DATA_BIT:;
            char c = iterator->data[iterator->index];

            if (last_data_bit_p(iterator)) {
                iterator->index++;
                iterator->state = STOP_BIT;
                iterator->data_bit_index = 0;
                return STREAM_ONE;
            } else {
             return (c >> iterator->data_bit_index++) & 1;
            }

        case STOP_BIT:
            if (end_of_data(iterator)) {
                iterator->state = ITERATOR_EOF;
                return STREAM_EOF;
            }

            iterator->state = START_BIT;
            return STREAM_ONE;
    }
}

void uart_interrupt_handler(void) {
    if (TMR2IF) {

        switch (next(&uart.bit_iterator)) {
            case STREAM_ZERO:
                RA2 = 0;
                break;
            case STREAM_ONE:
                RA2 = 1;
                break;
            case STREAM_EOF:
                uart_stop();
                break;
        }
        TMR2IF = 0;
    }
}
