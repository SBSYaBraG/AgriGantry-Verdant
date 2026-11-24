/* Test the compiled command parser and trajectory together using modeled I/O. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motor_test.h"
#include "multi_driver_diag.h"
#include "encoder_diag.h"
#include "pi_uart_diag.h"
#include "spi.h"
#include "usart.h"
#include "tim.h"

GPIO_TypeDef test_ports[6];
SPI_HandleTypeDef hspi3 = {SPI3, HAL_SPI_STATE_READY};
TIM_HandleTypeDef htim5;
static TestUSART uart_regs;
UART_HandleTypeDef huart1 = {&uart_regs};
volatile uint8_t encoder_diag_initialized;
volatile HAL_StatusTypeDef encoder_diag_timer_start_status = HAL_OK;
volatile uint32_t encoder_diag_index_event_count;
volatile MultiDriverDiagSummary multi_driver_diag_summary;
volatile MultiDriverDiagDriverResult multi_driver_diag_driver[MULTI_DRIVER_DIAG_DRIVER_COUNT];

static const char *scenario;
static char output[4096];
static const char *incoming = "";
static uint8_t registers[2][64], side_byte;
static uint32_t tick, next_beat, move_start, disabled_at;
static unsigned steps, gate_raises;
static bool in_move;

static bool is(const char *name) {return strcmp(scenario, name) == 0;}
void Encoder_Diagnostic_StartCounter(void) {encoder_diag_initialized = !is("no_encoder");}
void Multi_Driver_Diagnostic_Run(void) {assert(0 && "A trajectory must not trigger a scan");}
uint32_t HAL_GetTick(void) {return tick;}
void HAL_Delay(uint32_t ms)
{
  tick += ms + 1U;
  if (in_move && !side_byte && tick >= next_beat && !is("link_loss"))
  {
    next_beat = tick + 180U;
    side_byte = (is("stop") && steps >= 18U) ? '!' : '~';
    uart_regs.ISR |= UART_FLAG_RXNE;
  }
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pins, GPIO_PinState value)
{
  if (value) port->ODR |= pins; else port->ODR &= ~pins;
  if (port == ENA_X_MCU_GPIO_Port && (pins & ENA_X_MCU_Pin))
  {
    if (value) {
      ++gate_raises;
      for (unsigned chip = 0; chip < 2; ++chip) {
        assert(registers[chip][4] == 0x8F && registers[chip][0x10] == 0x12);
        assert(registers[chip][0x0E] == 0x19 && motor_pair_driver[chip].configured);
      }
    } else if (gate_raises) disabled_at = tick;
  }
  if (value && port == STEP_X_GPIO_Port && (pins & STEP_X_Pin))
  {
    assert(ENA_X_MCU_GPIO_Port->ODR & ENA_X_MCU_Pin);
    assert(SLEEP_X_GPIO_Port->ODR & SLEEP_X_Pin);
    ++steps;
    htim5.count = 0U - steps * 4000U / 3200U;
    encoder_diag_index_event_count = steps / 3200U;
  }
  assert(!(ENA_Y_MCU_GPIO_Port->ODR & ENA_Y_MCU_Pin));
  assert(!(ENA_Z_MCU_GPIO_Port->ODR & ENA_Z_MCU_Pin));
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
  if (is("nfault_x2") && port == FAULT_X2_GPIO_Port && pin == FAULT_X2_Pin && steps >= 13)
    return GPIO_PIN_RESET;
  return GPIO_PIN_SET;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *h, uint8_t *tx, uint8_t *rx, uint16_t n, uint32_t wait)
{
  (void)h; (void)wait;
  bool x1 = !(GPIOE->ODR & CS_X1_Pin), x2 = !(GPIOE->ODR & CS_X2_Pin);
  assert(x1 != x2 && n == 2 && (CS_Y_GPIO_Port->ODR & CS_Y_Pin) && (CS_Z_GPIO_Port->ODR & CS_Z_Pin));
  unsigned chip = x2, reg = tx[0] & 0x3FU;
  rx[0] = 0xC0; rx[1] = registers[chip][reg];
  if (!(tx[0] & 0x40U)) {
    assert(!(ENA_X_MCU_GPIO_Port->ODR & ENA_X_MCU_Pin));
    bool reject_write = chip == 1 && ((is("readback_x2") && reg == 0x0E) ||
                                      (is("enable_x2") && reg == 4 && tx[1] == 0x8F));
    if (!reject_write)
      registers[chip][reg] = tx[1];
  } else if (chip == 1 && steps >= 100) {
    if (is("thermal_x2") && reg == 2) rx[1] = 0x40;
    if (is("spi_x2") && reg == 0) return HAL_TIMEOUT;
    if (is("open_x2") && reg == 2) rx[1] = 1;
    if (is("header_x2") && reg == 0) rx[0] = 0;
  }
  return HAL_OK;
}
uint32_t HAL_UART_GetError(UART_HandleTypeDef *h) {(void)h; return 0;}
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *h, uint8_t *data, uint16_t n, uint32_t wait)
{
  (void)h; (void)n; (void)wait;
  if (*incoming) *data = (uint8_t)*incoming++;
  else if (side_byte) {*data = side_byte; side_byte = 0;}
  else return HAL_TIMEOUT;
  if (!side_byte && !*incoming) uart_regs.ISR &= ~UART_FLAG_RXNE;
  return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, const uint8_t *data, uint16_t n, uint32_t wait)
{
  (void)h; (void)wait;
  assert(strlen(output) + n < sizeof(output));
  strncat(output, (const char *)data, n);
  if (strncmp((const char *)data, "MOVE X START", 12) == 0) {in_move = true; move_start = tick;}
  if (strncmp((const char *)data, "RESULT ", 7) == 0) in_move = false;
  return HAL_OK;
}
static void send_line(const char *line)
{
  incoming = line;
  uart_regs.ISR |= UART_FLAG_RXNE;
  while (*incoming) PiUart_Poll();
}
int main(int argc, char **argv)
{
  assert(argc == 2 || (argc == 3 && strcmp(argv[2], "--uart") == 0));
  scenario = argv[1];
  const char *expected = NULL;
  for (unsigned chip = 0; chip < 2; ++chip) {
    registers[chip][4] = 0x0F; registers[chip][5] = 6;
    registers[chip][0x10] = 0x10; registers[chip][0x0E] = 0xFF;
  }
  PiUart_Init();
  send_line("~!PAIR?\nMOVE X\nARM X1\nMOVE X\nARM X\n!MOVE X\nAR!M X\nMOVE X\nARM X\n");
  assert(!gate_raises && !steps && strstr(output,"REFUSED X send ARM X first"));
  if (is("expired")) tick += 30001;
  send_line("MOVE X\n");
  if (is("expired") || is("no_encoder")) {
    assert(!steps && !gate_raises && !strstr(output, "MOVE X START"));
    puts("No move admitted.");
    return 0;
  }
  if (is("ok")) {expected = "run=DONE steps=3200 enc_delta=-4000 z_delta=1 ab_range=OK"; assert(steps == 3200);}
  else if (is("readback_x2")) {expected = "run=PREP_FAILED"; assert(!gate_raises && !steps);}
  else if (is("enable_x2")) {expected = "run=ENABLE_FAILED"; assert(!gate_raises && !steps && registers[0][4] == 0x8F);}
  else if (is("thermal_x2")) {expected = "run=THERMAL"; assert(steps == 100);}
  else if (is("spi_x2") || is("header_x2")) {expected = "run=SPI_ERROR"; assert(steps == 100);}
  else if (is("open_x2")) {expected = "run=DRIVER_FAULT"; assert(steps == 100);}
  else if (is("nfault_x2")) {expected = "run=X2_NFAULT"; assert(steps == 13);}
  else if (is("stop")) {expected = "run=STOPPED"; assert(steps > 0 && steps < 100);}
  else if (is("link_loss")) {expected = "run=LINK_LOST"; assert(steps < 3200 && disabled_at - move_start <= 1230U);}
  assert(expected && strstr(output,expected));
  assert(strstr(output,"DRV X1 ") && strstr(output,"DRV X2 "));
  unsigned first_steps = steps;
  send_line("PAIR?\nARM X\nMOVE X\n");
  assert(strstr(output,"state=USED") && steps == first_steps);
  assert(!(ENA_X_MCU_GPIO_Port->ODR & ENA_X_MCU_Pin) && !(GPIOE->ODR & (SLEEP_X_Pin | STEP_X_Pin)));
  if (argc == 3) fputs(output, stdout); else printf("%s: %s\n", scenario, expected);
  return 0;
}
