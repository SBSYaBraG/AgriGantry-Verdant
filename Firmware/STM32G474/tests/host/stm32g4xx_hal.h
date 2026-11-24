/* Host-only HAL for exercising the firmware's UART, GPIO and SPI decisions. */
#ifndef TEST_STM32G4XX_HAL_H
#define TEST_STM32G4XX_HAL_H
#include <stddef.h>
#include <stdint.h>
typedef enum {HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;
typedef enum {GPIO_PIN_RESET, GPIO_PIN_SET} GPIO_PinState;
typedef struct {uint32_t ODR;} GPIO_TypeDef;
extern GPIO_TypeDef test_ports[6];
#define GPIOA (&test_ports[0])
#define GPIOC (&test_ports[2])
#define GPIOD (&test_ports[3])
#define GPIOE (&test_ports[4])
#define GPIOF (&test_ports[5])
#define GPIO_PIN_0 (1U << 0)
#define GPIO_PIN_1 (1U << 1)
#define GPIO_PIN_2 (1U << 2)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_7 (1U << 7)
#define GPIO_PIN_8 (1U << 8)
#define GPIO_PIN_9 (1U << 9)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_11 (1U << 11)
#define GPIO_PIN_12 (1U << 12)
#define GPIO_PIN_13 (1U << 13)
#define GPIO_PIN_14 (1U << 14)
#define GPIO_PIN_15 (1U << 15)
typedef struct {int Instance, State;} SPI_HandleTypeDef;
#define SPI3 3
#define HAL_SPI_STATE_READY 1
typedef struct {uint32_t count;} TIM_HandleTypeDef;
#define __HAL_TIM_GET_COUNTER(h) ((h)->count)
typedef struct {uint32_t ISR;} TestUSART;
typedef struct {TestUSART *Instance;} UART_HandleTypeDef;
#define UART_FLAG_PE 1U
#define UART_FLAG_FE 2U
#define UART_FLAG_NE 4U
#define UART_FLAG_ORE 8U
#define UART_FLAG_RXNE 32U
#define UART_CLEAR_PEF UART_FLAG_PE
#define UART_CLEAR_FEF UART_FLAG_FE
#define UART_CLEAR_NEF UART_FLAG_NE
#define UART_CLEAR_OREF UART_FLAG_ORE
#define HAL_UART_ERROR_NONE 0U
#define __HAL_UART_CLEAR_FLAG(h, mask) ((h)->Instance->ISR &= ~(mask))
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pins, GPIO_PinState value);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *, uint8_t *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *, const uint8_t *, uint16_t, uint32_t);
uint32_t HAL_UART_GetError(UART_HandleTypeDef *);
#endif
