/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : multi_driver_diag.c
  * @brief          : Test 11 read-only all-DRV8461 SPI communication scan.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "multi_driver_diag.h"

#include <string.h>

#include "spi.h"

#define DRV8461_REG_FAULT                       0x00U
#define DRV8461_REG_CTRL1                       0x04U
#define DRV8461_REG_CTRL2                       0x05U
#define DRV8461_CTRL1_RESET_VALUE               0x0FU
#define DRV8461_CTRL2_RESET_VALUE               0x06U
#define DRV8461_READ_COMMAND_BIT                0x40U
#define DRV8461_STATUS_HEADER_MASK              0xC0U
#define DRV8461_STATUS_HEADER_VALUE             0xC0U
#define DRV8461_STATUS_FAULT_MASK               0x3FU
#define DRV8461_SPI_TIMEOUT_MS                  10U
#define DRV8461_SLEEP_RESET_DELAY_MS            1U
#define DRV8461_EARLY_FAULT_SAMPLE_DELAY_MS     2U
#define DRV8461_STARTUP_SETTLE_DELAY_MS         30U
#define DRV8461_FAULT_RECHECK_DELAY_MS          10U
#define DRV8461_FRAME_GAP_MS                    1U
#define MULTI_DRIVER_DIAG_SIGNATURE             0x4D445631UL

#define MULTI_DRIVER_LED_PASS_FLASH_MS          90U
#define MULTI_DRIVER_LED_DIAG_ON_MS             300U
#define MULTI_DRIVER_LED_DIAG_OFF_MS            200U
#define MULTI_DRIVER_LED_FAIL_ON_MS             1000U
#define MULTI_DRIVER_LED_PATTERN_PAUSE_MS       2000U

typedef struct
{
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
  GPIO_TypeDef *fault_port;
  uint16_t fault_pin;
} DriverEndpoint;

volatile MultiDriverDiagSummary multi_driver_diag_summary;
volatile MultiDriverDiagDriverResult
    multi_driver_diag_driver[MULTI_DRIVER_DIAG_DRIVER_COUNT];

static const DriverEndpoint multi_driver_endpoints[MULTI_DRIVER_DIAG_DRIVER_COUNT] =
{
  {CS_X1_GPIO_Port, CS_X1_Pin, FAULT_X1_GPIO_Port, FAULT_X1_Pin},
  {CS_X2_GPIO_Port, CS_X2_Pin, FAULT_X2_GPIO_Port, FAULT_X2_Pin},
  {CS_Y_GPIO_Port,  CS_Y_Pin,  FAULT_Y_GPIO_Port,  FAULT_Y_Pin},
  {CS_Z_GPIO_Port,  CS_Z_Pin,  FAULT_Z_GPIO_Port,  FAULT_Z_Pin}
};

static void Multi_Driver_Diagnostic_ClearResults(void);
static void Multi_Driver_Diagnostic_ForceSafeState(void);
static uint32_t Multi_Driver_Diagnostic_CaptureFaultLevels(uint8_t sample_kind);
static uint8_t Multi_Driver_Diagnostic_ReadRegister(uint8_t driver_index,
                                                      uint8_t address,
                                                      volatile uint32_t *raw_frame);
static void Multi_Driver_Diagnostic_ReadFirstSet(uint8_t driver_index);
static void Multi_Driver_Diagnostic_ReadFaultRecheck(uint8_t driver_index);
static void Multi_Driver_Diagnostic_EvaluateDriver(uint8_t driver_index);
static uint8_t Multi_Driver_Diagnostic_IsInvalidRawFrame(uint32_t raw_frame);
static uint8_t Multi_Driver_Diagnostic_HasValidStatusHeader(uint32_t raw_frame);
static void Multi_Driver_Diagnostic_Shutdown(void);
static void Multi_Driver_Diagnostic_IndicatePass(void);
static void Multi_Driver_Diagnostic_IndicateDiagnostic(void);
static void Multi_Driver_Diagnostic_IndicateCommunicationFailure(void);

/**
 * @brief  Run the Test 11 scan once, then leave every DRV8461 asleep/disabled.
 * @note   Only standard 16-bit SPI read commands are sent.  This function does
 *         not write any DRV8461 register, alter MOTION_OK, or emit STEP pulses.
 */
void Multi_Driver_Diagnostic_Run(void)
{
  uint8_t driver_index;
  uint32_t fault_mask;

  Multi_Driver_Diagnostic_ClearResults();
  multi_driver_diag_summary.started = 1U;

  /* Reassert independent output barriers before any driver is woken. */
  Multi_Driver_Diagnostic_ForceSafeState();
  HAL_Delay(DRV8461_SLEEP_RESET_DELAY_MS);

  /* MX_SPI3_Init() is CubeMX-generated and runs before this application code. */
  multi_driver_diag_summary.spi_init_status =
      ((hspi3.Instance == SPI3) && (hspi3.State == HAL_SPI_STATE_READY)) ?
          (uint32_t)HAL_OK : (uint32_t)HAL_ERROR;
  if (multi_driver_diag_summary.spi_init_status != (uint32_t)HAL_OK)
  {
    multi_driver_diag_summary.overall_result = MULTI_DRIVER_DIAG_RESULT_COMM_FAILURE;
    Multi_Driver_Diagnostic_Shutdown();
    multi_driver_diag_summary.completed = 1U;
    return;
  }

  /* U4/U5 share SLEEP_X; each other driver has its own sleep command. */
  HAL_GPIO_WritePin(SLEEP_X_GPIO_Port, SLEEP_X_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(SLEEP_Y_GPIO_Port, SLEEP_Y_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(SLEEP_Z_GPIO_Port, SLEEP_Z_Pin, GPIO_PIN_SET);
  multi_driver_diag_summary.wake_tick_ms = HAL_GetTick();

  /* Preserve early post-wake nFAULT evidence without selecting any SPI device. */
  HAL_Delay(DRV8461_EARLY_FAULT_SAMPLE_DELAY_MS);
  multi_driver_diag_summary.early_fault_sample_tick_ms = HAL_GetTick();
  fault_mask = Multi_Driver_Diagnostic_CaptureFaultLevels(0U);
  multi_driver_diag_summary.early_nfault_asserted_mask = fault_mask;

  /* First SPI transaction begins at least 32 ms after nSLEEP was raised. */
  HAL_Delay(DRV8461_STARTUP_SETTLE_DELAY_MS);
  multi_driver_diag_summary.settled_fault_sample_tick_ms = HAL_GetTick();
  fault_mask = Multi_Driver_Diagnostic_CaptureFaultLevels(1U);
  multi_driver_diag_summary.settled_nfault_asserted_mask = fault_mask;

  for (driver_index = 0U; driver_index < MULTI_DRIVER_DIAG_DRIVER_COUNT; ++driver_index)
  {
    Multi_Driver_Diagnostic_ReadFirstSet(driver_index);
  }

  multi_driver_diag_summary.fault_recheck_delay_tick_ms = HAL_GetTick();
  HAL_Delay(DRV8461_FAULT_RECHECK_DELAY_MS);

  for (driver_index = 0U; driver_index < MULTI_DRIVER_DIAG_DRIVER_COUNT; ++driver_index)
  {
    Multi_Driver_Diagnostic_ReadFaultRecheck(driver_index);
  }

  fault_mask = Multi_Driver_Diagnostic_CaptureFaultLevels(2U);
  multi_driver_diag_summary.final_nfault_asserted_mask = fault_mask;

  for (driver_index = 0U; driver_index < MULTI_DRIVER_DIAG_DRIVER_COUNT; ++driver_index)
  {
    Multi_Driver_Diagnostic_EvaluateDriver(driver_index);
  }

  if (multi_driver_diag_summary.communication_failure_mask != 0U)
  {
    multi_driver_diag_summary.overall_result = MULTI_DRIVER_DIAG_RESULT_COMM_FAILURE;
  }
  else if ((multi_driver_diag_summary.diagnostic_mask != 0U) ||
           (multi_driver_diag_summary.control_default_mismatch_mask != 0U))
  {
    /* A fault/default anomaly is distinct from a broken SPI communication link. */
    multi_driver_diag_summary.overall_result = MULTI_DRIVER_DIAG_RESULT_DIAGNOSTIC;
  }
  else
  {
    multi_driver_diag_summary.overall_result = MULTI_DRIVER_DIAG_RESULT_PASS;
  }

  Multi_Driver_Diagnostic_Shutdown();
  multi_driver_diag_summary.completed = 1U;
}

/**
 * @brief  Display the completed Test 11 result without changing driver state.
 * @note   PASS = four rapid flashes; diagnostic anomaly = two medium flashes;
 *         communication failure = one long flash.  Each pattern has 2 s dark.
 */
void Multi_Driver_Diagnostic_Service(void)
{
  if (multi_driver_diag_summary.overall_result == MULTI_DRIVER_DIAG_RESULT_PASS)
  {
    Multi_Driver_Diagnostic_IndicatePass();
  }
  else if (multi_driver_diag_summary.overall_result == MULTI_DRIVER_DIAG_RESULT_DIAGNOSTIC)
  {
    Multi_Driver_Diagnostic_IndicateDiagnostic();
  }
  else
  {
    Multi_Driver_Diagnostic_IndicateCommunicationFailure();
  }
}

static void Multi_Driver_Diagnostic_ClearResults(void)
{
  uint8_t driver_index;

  (void)memset((void *)&multi_driver_diag_summary, 0, sizeof(multi_driver_diag_summary));
  (void)memset((void *)multi_driver_diag_driver, 0, sizeof(multi_driver_diag_driver));

  multi_driver_diag_summary.signature = MULTI_DRIVER_DIAG_SIGNATURE;
  multi_driver_diag_summary.overall_result = MULTI_DRIVER_DIAG_RESULT_NOT_RUN;
  multi_driver_diag_summary.spi_init_status = MULTI_DRIVER_DIAG_STATUS_NOT_RUN;

  for (driver_index = 0U; driver_index < MULTI_DRIVER_DIAG_DRIVER_COUNT; ++driver_index)
  {
    multi_driver_diag_driver[driver_index].fault_first_hal_status = MULTI_DRIVER_DIAG_STATUS_NOT_RUN;
    multi_driver_diag_driver[driver_index].ctrl1_hal_status = MULTI_DRIVER_DIAG_STATUS_NOT_RUN;
    multi_driver_diag_driver[driver_index].ctrl2_hal_status = MULTI_DRIVER_DIAG_STATUS_NOT_RUN;
    multi_driver_diag_driver[driver_index].fault_recheck_hal_status = MULTI_DRIVER_DIAG_STATUS_NOT_RUN;
    multi_driver_diag_driver[driver_index].raw_frame_invalid = 1U;
  }
}

static void Multi_Driver_Diagnostic_ForceSafeState(void)
{
  HAL_GPIO_WritePin(GPIOE, CS_X1_Pin | CS_X2_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Y_GPIO_Port, CS_Y_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Z_GPIO_Port, CS_Z_Pin, GPIO_PIN_SET);

  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Y_MCU_GPIO_Port, ENA_Y_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Z_MCU_GPIO_Port, ENA_Z_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, DIR_X_Pin | STEP_X_Pin | SLEEP_X_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, SLEEP_Y_Pin | SLEEP_Z_Pin, GPIO_PIN_RESET);
}

static uint32_t Multi_Driver_Diagnostic_CaptureFaultLevels(uint8_t sample_kind)
{
  uint8_t driver_index;
  uint8_t level;
  uint32_t asserted_mask = 0U;

  for (driver_index = 0U; driver_index < MULTI_DRIVER_DIAG_DRIVER_COUNT; ++driver_index)
  {
    level = (uint8_t)HAL_GPIO_ReadPin(multi_driver_endpoints[driver_index].fault_port,
                                      multi_driver_endpoints[driver_index].fault_pin);
    if (sample_kind == 0U)
    {
      multi_driver_diag_driver[driver_index].early_nfault_level = level;
    }
    else if (sample_kind == 1U)
    {
      multi_driver_diag_driver[driver_index].settled_nfault_level = level;
    }
    else
    {
      multi_driver_diag_driver[driver_index].final_nfault_level = level;
    }

    if (level == (uint8_t)GPIO_PIN_RESET)
    {
      asserted_mask |= (1UL << driver_index);
    }
  }

  return asserted_mask;
}

static uint8_t Multi_Driver_Diagnostic_ReadRegister(uint8_t driver_index,
                                                      uint8_t address,
                                                      volatile uint32_t *raw_frame)
{
  uint8_t tx_data[2] = {(uint8_t)(DRV8461_READ_COMMAND_BIT | (address & 0x3FU)), 0x00U};
  uint8_t rx_data[2] = {0U, 0U};
  HAL_StatusTypeDef transfer_status;

  /* Exactly one active-low CS is asserted for each standard 16-bit read frame. */
  HAL_GPIO_WritePin(multi_driver_endpoints[driver_index].cs_port,
                    multi_driver_endpoints[driver_index].cs_pin,
                    GPIO_PIN_RESET);
  transfer_status = HAL_SPI_TransmitReceive(&hspi3, tx_data, rx_data, 2U,
                                             DRV8461_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(multi_driver_endpoints[driver_index].cs_port,
                    multi_driver_endpoints[driver_index].cs_pin,
                    GPIO_PIN_SET);
  HAL_Delay(DRV8461_FRAME_GAP_MS);

  *raw_frame = ((uint32_t)rx_data[0] << 8U) | (uint32_t)rx_data[1];
  return (uint8_t)transfer_status;
}

static void Multi_Driver_Diagnostic_ReadFirstSet(uint8_t driver_index)
{
  volatile MultiDriverDiagDriverResult *result = &multi_driver_diag_driver[driver_index];

  result->fault_first_tick_ms = HAL_GetTick();
  result->fault_first_hal_status = Multi_Driver_Diagnostic_ReadRegister(
      driver_index, DRV8461_REG_FAULT, &result->fault_first_raw_frame);
  result->fault_first_value = (uint8_t)result->fault_first_raw_frame;

  result->ctrl1_tick_ms = HAL_GetTick();
  result->ctrl1_hal_status = Multi_Driver_Diagnostic_ReadRegister(
      driver_index, DRV8461_REG_CTRL1, &result->ctrl1_raw_frame);
  result->ctrl1_value = (uint8_t)result->ctrl1_raw_frame;

  result->ctrl2_tick_ms = HAL_GetTick();
  result->ctrl2_hal_status = Multi_Driver_Diagnostic_ReadRegister(
      driver_index, DRV8461_REG_CTRL2, &result->ctrl2_raw_frame);
  result->ctrl2_value = (uint8_t)result->ctrl2_raw_frame;
}

static void Multi_Driver_Diagnostic_ReadFaultRecheck(uint8_t driver_index)
{
  volatile MultiDriverDiagDriverResult *result = &multi_driver_diag_driver[driver_index];

  result->fault_recheck_tick_ms = HAL_GetTick();
  result->fault_recheck_hal_status = Multi_Driver_Diagnostic_ReadRegister(
      driver_index, DRV8461_REG_FAULT, &result->fault_recheck_raw_frame);
  result->fault_recheck_value = (uint8_t)result->fault_recheck_raw_frame;
}

static void Multi_Driver_Diagnostic_EvaluateDriver(uint8_t driver_index)
{
  volatile MultiDriverDiagDriverResult *result = &multi_driver_diag_driver[driver_index];
  uint8_t header_mask = 0U;
  uint8_t communication_valid;
  uint8_t diagnostic_indicated;

  if (Multi_Driver_Diagnostic_HasValidStatusHeader(result->fault_first_raw_frame) != 0U)
  {
    header_mask |= 0x01U;
  }
  if (Multi_Driver_Diagnostic_HasValidStatusHeader(result->ctrl1_raw_frame) != 0U)
  {
    header_mask |= 0x02U;
  }
  if (Multi_Driver_Diagnostic_HasValidStatusHeader(result->ctrl2_raw_frame) != 0U)
  {
    header_mask |= 0x04U;
  }
  if (Multi_Driver_Diagnostic_HasValidStatusHeader(result->fault_recheck_raw_frame) != 0U)
  {
    header_mask |= 0x08U;
  }
  result->valid_status_header_mask = header_mask;

  result->status_fault_bits_or = (uint8_t)((((uint8_t)(result->fault_first_raw_frame >> 8U)) |
                                             ((uint8_t)(result->ctrl1_raw_frame >> 8U)) |
                                             ((uint8_t)(result->ctrl2_raw_frame >> 8U)) |
                                             ((uint8_t)(result->fault_recheck_raw_frame >> 8U))) &
                                            DRV8461_STATUS_FAULT_MASK);
  result->raw_frame_invalid =
      ((Multi_Driver_Diagnostic_IsInvalidRawFrame(result->fault_first_raw_frame) != 0U) ||
       (Multi_Driver_Diagnostic_IsInvalidRawFrame(result->ctrl1_raw_frame) != 0U) ||
       (Multi_Driver_Diagnostic_IsInvalidRawFrame(result->ctrl2_raw_frame) != 0U) ||
       (Multi_Driver_Diagnostic_IsInvalidRawFrame(result->fault_recheck_raw_frame) != 0U)) ? 1U : 0U;

  communication_valid =
      ((result->fault_first_hal_status == (uint8_t)HAL_OK) &&
       (result->ctrl1_hal_status == (uint8_t)HAL_OK) &&
       (result->ctrl2_hal_status == (uint8_t)HAL_OK) &&
       (result->fault_recheck_hal_status == (uint8_t)HAL_OK) &&
       (result->valid_status_header_mask == 0x0FU) &&
       (result->raw_frame_invalid == 0U)) ? 1U : 0U;
  result->communication_valid = communication_valid;

  result->control_defaults_valid =
      ((result->ctrl1_value == DRV8461_CTRL1_RESET_VALUE) &&
       (result->ctrl2_value == DRV8461_CTRL2_RESET_VALUE)) ? 1U : 0U;

  diagnostic_indicated =
      ((result->fault_first_value != 0U) ||
       (result->fault_recheck_value != 0U) ||
       (result->status_fault_bits_or != 0U) ||
       (result->early_nfault_level == (uint8_t)GPIO_PIN_RESET) ||
       (result->settled_nfault_level == (uint8_t)GPIO_PIN_RESET) ||
       (result->final_nfault_level == (uint8_t)GPIO_PIN_RESET)) ? 1U : 0U;
  result->diagnostic_indicated = diagnostic_indicated;

  if (communication_valid == 0U)
  {
    multi_driver_diag_summary.communication_failure_mask |= (1UL << driver_index);
  }
  if (diagnostic_indicated != 0U)
  {
    multi_driver_diag_summary.diagnostic_mask |= (1UL << driver_index);
  }
  if (result->control_defaults_valid == 0U)
  {
    multi_driver_diag_summary.control_default_mismatch_mask |= (1UL << driver_index);
  }
}

static uint8_t Multi_Driver_Diagnostic_IsInvalidRawFrame(uint32_t raw_frame)
{
  return ((raw_frame == 0x0000U) || (raw_frame == 0xFFFFU)) ? 1U : 0U;
}

static uint8_t Multi_Driver_Diagnostic_HasValidStatusHeader(uint32_t raw_frame)
{
  return ((((uint8_t)(raw_frame >> 8U)) & DRV8461_STATUS_HEADER_MASK) ==
          DRV8461_STATUS_HEADER_VALUE) ? 1U : 0U;
}

static void Multi_Driver_Diagnostic_Shutdown(void)
{
  Multi_Driver_Diagnostic_ForceSafeState();
  multi_driver_diag_summary.shutdown_tick_ms = HAL_GetTick();
  multi_driver_diag_summary.safe_shutdown_confirmed = 1U;
}

static void Multi_Driver_Diagnostic_IndicatePass(void)
{
  uint8_t flash_count;

  for (flash_count = 0U; flash_count < 4U; ++flash_count)
  {
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
    HAL_Delay(MULTI_DRIVER_LED_PASS_FLASH_MS);
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
    HAL_Delay(MULTI_DRIVER_LED_PASS_FLASH_MS);
  }
  HAL_Delay(MULTI_DRIVER_LED_PATTERN_PAUSE_MS);
}

static void Multi_Driver_Diagnostic_IndicateDiagnostic(void)
{
  uint8_t flash_count;

  for (flash_count = 0U; flash_count < 2U; ++flash_count)
  {
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
    HAL_Delay(MULTI_DRIVER_LED_DIAG_ON_MS);
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
    HAL_Delay(MULTI_DRIVER_LED_DIAG_OFF_MS);
  }
  HAL_Delay(MULTI_DRIVER_LED_PATTERN_PAUSE_MS);
}

static void Multi_Driver_Diagnostic_IndicateCommunicationFailure(void)
{
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
  HAL_Delay(MULTI_DRIVER_LED_FAIL_ON_MS);
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
  HAL_Delay(MULTI_DRIVER_LED_PATTERN_PAUSE_MS);
}
