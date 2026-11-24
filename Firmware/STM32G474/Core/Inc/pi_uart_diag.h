#ifndef PI_UART_DIAG_H
#define PI_UART_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the X1 counter after the generated TIM5 and USART1 initializers. */
void PiUart_Init(void);

/* Serve one request at a time from the main loop. */
void PiUart_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* PI_UART_DIAG_H */
