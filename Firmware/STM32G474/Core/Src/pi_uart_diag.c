/* Test 15: telemetry and one explicitly armed revolution per boot (X or legacy X1). */
#include "pi_uart_diag.h"
#include "encoder_diag.h"
#include "motor_test.h"
#include "multi_driver_diag.h"
#include "tim.h"
#include "usart.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define ARM_WINDOW_MS 30000U
#define PAIRED_HEARTBEAT_MS 1200U
static enum { X1_IDLE, X1_ARMED, X1_USED } x1_state;
static uint32_t armed_at_ms;
static MotorMoveResult last_move;
static bool paired_target, paired_link_lost;
static uint32_t paired_heartbeat_ms;

/* Optional SWD counters. The Pi terminal carries the actual replies. */
static volatile struct {
  uint32_t rx_bytes;
  uint32_t pings_received;
  uint32_t pongs_sent;
  uint32_t rx_errors;
  uint32_t tx_errors;
} pi_uart_stats;

void PiUart_Init(void)
{
  Encoder_Diagnostic_StartCounter();
}

static bool Reply(const char *format, ...) __attribute__((format(printf, 1, 2)));

static bool Reply(const char *format, ...)
{
  char line[128];
  va_list args;
  va_start(args, format);
  int length = vsnprintf(line, sizeof(line), format, args);
  va_end(args);

  if (length < 0 || (size_t)length >= sizeof(line) ||
      HAL_UART_Transmit(&huart1, (const uint8_t *)line, (uint16_t)length, 50U) != HAL_OK)
  {
    ++pi_uart_stats.tx_errors;
    return false;
  }
  return true;
}

static unsigned OutputCommand(const GPIO_TypeDef *port, uint16_t pin)
{
  return (port->ODR & pin) != 0U;
}

static void SendStatus(void)
{
  char encoder_count[sizeof("-2147483648")] = "NA";
  if (encoder_diag_initialized)
  {
    int32_t count = (int32_t)__HAL_TIM_GET_COUNTER(&htim5);
    (void)snprintf(encoder_count, sizeof(encoder_count), "%ld", (long)count);
  }

  (void)Reply("STATUS uptime_ms=%lu ENA_X=%u ENA_Y=%u ENA_Z=%u ENC_X1=%s\r\n",
              (unsigned long)HAL_GetTick(),
              OutputCommand(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin),
              OutputCommand(ENA_Y_MCU_GPIO_Port, ENA_Y_MCU_Pin),
              OutputCommand(ENA_Z_MCU_GPIO_Port, ENA_Z_MCU_Pin), encoder_count);
}

static bool SendDriverResult(unsigned index)
{
  static const char *const names[MULTI_DRIVER_DIAG_DRIVER_COUNT] =
      {"U4/X1", "U5/X2", "U6/Y", "U7/Z"};
  const char *name = names[index];
  const volatile MultiDriverDiagDriverResult *result = &multi_driver_diag_driver[index];

  if (multi_driver_diag_summary.spi_init_status != HAL_OK)
  {
    return Reply("SCAN %s SPI=NOT_RUN FAULT=NA nFAULT=NA\r\n", name);
  }
  unsigned early = result->early_nfault_level;
  unsigned settled = result->settled_nfault_level;
  unsigned final = result->final_nfault_level;
  if (!result->communication_valid)
  {
    return Reply("SCAN %s SPI=FAIL FAULT=NA nFAULT=%u/%u/%u\r\n",
                 name, early, settled, final);
  }
  return Reply("SCAN %s SPI=OK FAULT=0x%02X/0x%02X nFAULT=%u/%u/%u "
               "STATUS_BITS=0x%02X CTRL1=0x%02X CTRL2=0x%02X\r\n",
               name, (unsigned)result->fault_first_value, (unsigned)result->fault_recheck_value,
               early, settled, final, (unsigned)result->status_fault_bits_or,
               (unsigned)result->ctrl1_value, (unsigned)result->ctrl2_value);
}

static bool ReceiveFailed(HAL_StatusTypeDef status)
{
  uint32_t errors = huart1.Instance->ISR & (UART_FLAG_PE | UART_FLAG_FE | UART_FLAG_NE | UART_FLAG_ORE);
  if (errors == 0U && HAL_UART_GetError(&huart1) == HAL_UART_ERROR_NONE &&
      (status == HAL_OK || status == HAL_TIMEOUT))
    return false;

  __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_PEF | UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_OREF);
  ++pi_uart_stats.rx_errors;
  return true;
}

/* During a move, a new input byte or UART error is a stop request. A trailing
 * CR/LF from the accepted command is harmless. Never dispatch a queued move. */
static bool StopRequested(void)
{
  if ((huart1.Instance->ISR & UART_FLAG_RXNE) == 0U)
    return ReceiveFailed(HAL_TIMEOUT);
  uint8_t byte;
  HAL_StatusTypeDef status = HAL_UART_Receive(&huart1, &byte, 1U, 0U);
  if (ReceiveFailed(status) || status != HAL_OK)
    return true;
  ++pi_uart_stats.rx_bytes;
  return byte != '\r' && byte != '\n';
}

static bool PairedStopRequested(void)
{
  if ((uint32_t)(HAL_GetTick() - paired_heartbeat_ms) >= PAIRED_HEARTBEAT_MS || ReceiveFailed(HAL_TIMEOUT))
  {
    paired_link_lost = true;
    return true;
  }
  if ((huart1.Instance->ISR & UART_FLAG_RXNE) == 0U) return false;
  uint8_t byte;
  HAL_StatusTypeDef status = HAL_UART_Receive(&huart1, &byte, 1U, 0U);
  if (status != HAL_OK || ReceiveFailed(status))
  {
    paired_link_lost = true;
    return true;
  }
  if (byte == '~')
    paired_heartbeat_ms = HAL_GetTick();
  else if (byte != '\r' && byte != '\n')
    return true;
  return false;
}

static void SendMoveResult(void)
{
  static const char *const names[] = {
    [MOTOR_MOVE_DONE] = "DONE", [MOTOR_MOVE_STOPPED] = "STOPPED",
    [MOTOR_MOVE_ENCODER_UNAVAILABLE] = "ENCODER_UNAVAILABLE",
    [MOTOR_MOVE_PREPARATION_FAILED] = "PREP_FAILED", [MOTOR_MOVE_ENABLE_FAILED] = "ENABLE_FAILED",
    [MOTOR_MOVE_X1_NFAULT] = "X1_NFAULT", [MOTOR_MOVE_X2_NFAULT] = "X2_NFAULT",
    [MOTOR_MOVE_SPI_ERROR] = "SPI_ERROR", [MOTOR_MOVE_DRIVER_FAULT] = "DRIVER_FAULT",
    [MOTOR_MOVE_THERMAL] = "THERMAL", [MOTOR_MOVE_LINK_LOST] = "LINK_LOST"
  };
  if (x1_state != X1_USED)
  {
    (void)Reply("RESULT X1 run=NONE\r\n");
    return;
  }

  const char *axis = paired_target ? "X" : "X1";
  if (motor_test_encoder_window_started)
  {
    const char *range = "NA";
    if (last_move == MOTOR_MOVE_DONE)
      range = motor_test_encoder_count_within_tolerance ? "OK" : "OUT";
    if (!Reply("RESULT %s run=%s steps=%u enc_delta=%ld z_delta=%lu ab_range=%s ms=%lu\r\n",
               axis, names[last_move], (unsigned)motor_test_one_rev_pulses_issued,
               (long)motor_test_encoder_delta_total, (unsigned long)motor_test_encoder_index_delta,
               range, (unsigned long)(motor_test_one_rev_end_tick_ms - motor_test_one_rev_start_tick_ms)))
      return;
  }
  else if (!Reply("RESULT %s run=%s steps=0 enc_delta=NA z_delta=NA\r\n", axis, names[last_move]))
    return;

  if (paired_target)
  {
    for (unsigned chip = 0; chip < 2U; ++chip)
    {
      const MotorXDriverReport *d = &motor_pair_driver[chip];
      if (!Reply("DRV X%u cfg=%u ctrl1=0x%02X n=%u fault=0x%04X diag2=0x%04X reg=0x%02X hal=%u raw=0x%04X\r\n",
                 chip + 1U, (unsigned)d->configured, (unsigned)d->ctrl1, (unsigned)d->samples,
                 (unsigned)d->fault_frame, (unsigned)d->diag2_frame, (unsigned)d->last_reg,
                 (unsigned)d->last_hal, (unsigned)d->last_frame)) return;
    }
    return;
  }
  if (motor_test_one_rev_diagnostic_sample_count == 0U)
    (void)Reply("DRV X1 samples=0 prep_mask=0x%02lX\r\n", (unsigned long)motor_test_failure_mask);
  else
    (void)Reply("DRV X1 samples=%u FAULT_raw=0x%04X DIAG2_raw=0x%04X HAL=%u/%u OTW=%u OTS=%u\r\n",
                (unsigned)motor_test_one_rev_diagnostic_sample_count,
                (unsigned)motor_test_one_rev_fault_raw_frame, (unsigned)motor_test_one_rev_diag2_raw_frame,
                (unsigned)motor_test_one_rev_fault_read_status, (unsigned)motor_test_one_rev_diag2_read_status,
                (unsigned)motor_test_one_rev_otw_seen, (unsigned)motor_test_one_rev_ots_seen);
}

static void StartMove(bool paired)
{
  const char *axis = paired ? "X" : "X1";
  if (x1_state == X1_USED)
  {
    (void)Reply("REFUSED %s already_used (send RESULT)\r\n", axis);
    return;
  }
  if (x1_state != X1_ARMED || paired_target != paired || (uint32_t)(HAL_GetTick() - armed_at_ms) >= ARM_WINDOW_MS)
  {
    x1_state = X1_IDLE;
    (void)Reply("REFUSED %s send ARM %s first\r\n", axis, axis);
    return;
  }
  x1_state = X1_IDLE; /* Consume the arm even if the acknowledgement cannot be sent. */
  if (!Reply("MOVE %s START\r\n", axis))
    return;
  x1_state = X1_USED;
  if (paired)
  {
    paired_link_lost = false;
    paired_heartbeat_ms = HAL_GetTick();
    last_move = Motor_Test_RunPairedRevolution(PairedStopRequested);
    if (last_move == MOTOR_MOVE_STOPPED && paired_link_lost) last_move = MOTOR_MOVE_LINK_LOST;
  }
  else
    last_move = Motor_Test_RunSupervisedRevolution(StopRequested);
  SendMoveResult();
}

static void HandleCommand(const char *command)
{
  if (strcmp(command, "MOVE X1") == 0 || strcmp(command, "MOVE X") == 0)
  {
    StartMove(strcmp(command, "MOVE X") == 0);
    return;
  }
  /* Any other nonempty command cancels an outstanding arm. */
  if (x1_state == X1_ARMED)
    x1_state = X1_IDLE;
  if (strcmp(command, "PING") == 0)
  {
    ++pi_uart_stats.pings_received;
    if (Reply("PONG\r\n"))
    {
      ++pi_uart_stats.pongs_sent;
    }
  }
  else if (strcmp(command, "STATUS") == 0)
  {
    SendStatus();
  }
  else if (strcmp(command, "PAIR?") == 0)
    (void)Reply("PAIR X v=1 state=%s\r\n", x1_state == X1_USED ? "USED" : "READY");
  else if (strcmp(command, "SCAN") == 0)
  {
    /* The Test 11 scan returns all drivers to sleep before the first reply. */
    Multi_Driver_Diagnostic_Run();
    for (unsigned i = 0; i < MULTI_DRIVER_DIAG_DRIVER_COUNT; ++i)
    {
      if (!SendDriverResult(i))
      {
        break;
      }
    }
  }
  else if (strcmp(command, "ARM X1") == 0 || strcmp(command, "ARM X") == 0)
  {
    bool paired = strcmp(command, "ARM X") == 0;
    const char *axis = paired ? "X" : "X1";
    if (x1_state == X1_USED)
      (void)Reply("REFUSED %s already_used (send RESULT)\r\n", axis);
    else if (!encoder_diag_initialized)
      (void)Reply("REFUSED %s encoder_unavailable\r\n", axis);
    else if (Reply("ARMED %s window_ms=%u\r\n", axis, (unsigned)ARM_WINDOW_MS))
    {
      paired_target = paired;
      armed_at_ms = HAL_GetTick();
      x1_state = X1_ARMED;
    }
  }
  else if (strcmp(command, "RESULT") == 0)
    SendMoveResult();
  else if (strcmp(command, "STOP") == 0)
    (void)Reply("STOP X1 idle\r\n");
}

void PiUart_Poll(void)
{
  static char command[sizeof("MOVE X1")]; /* Longest command plus its terminator. */
  static size_t used; /* sizeof(command) means discard through the next line ending. */
  uint8_t byte;
  HAL_StatusTypeDef status = HAL_UART_Receive(&huart1, &byte, 1U, 20U);
  if (x1_state == X1_ARMED && (uint32_t)(HAL_GetTick() - armed_at_ms) >= ARM_WINDOW_MS)
    x1_state = X1_IDLE;
  if (ReceiveFailed(status))
  {
    if (x1_state == X1_ARMED)
      x1_state = X1_IDLE;
    used = sizeof(command);
    return;
  }
  if (status == HAL_TIMEOUT)
  {
    return; /* A pause between characters preserves the partial line. */
  }

  ++pi_uart_stats.rx_bytes;
  if (byte == '\r' || byte == '\n')
  {
    if (used > 0U && used < sizeof(command))
    {
      command[used] = '\0';
      HandleCommand(command);
    }
    used = 0U;
  }
  else if (used == 0U && byte == '~')
  {
    /* A final host heartbeat can cross the result in flight. */
  }
  else if (byte == '!')
  {
    /* A standalone stop can also arrive just before or after a move. */
    if (x1_state == X1_ARMED) x1_state = X1_IDLE;
    if (used != 0U) used = sizeof(command); /* Discard an interrupted line. */
  }
  else if (used < sizeof(command) - 1U && byte != '\0')
  {
    command[used++] = (char)byte;
  }
  else
  {
    if (x1_state == X1_ARMED)
      x1_state = X1_IDLE;
    used = sizeof(command);
  }
}
