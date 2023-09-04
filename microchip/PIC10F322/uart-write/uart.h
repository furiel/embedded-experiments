#ifndef UART_H
#define	UART_H

void uart_init(void);
void uart_write(unsigned char *, int length);
void uart_interrupt_handler(void);

#endif	/* UART_H */
