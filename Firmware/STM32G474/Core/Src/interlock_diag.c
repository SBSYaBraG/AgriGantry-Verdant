/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : interlock_diag.c
  * @brief          : Supervised safe test of the X-axis enable interlock.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "interlock_diag.h"

#define INTERLOCK_SLEEP_RESET_MS       1U
#define INTERLOCK_AWAKE_SETTLE_MS      25U
#define INTERLOCK_ENABLE_PHASE_MS      2000U
#define INTERLOCK_FAULT_ON_MS          1000U
#define INTERLOCK_FAULT_OFF_MS         2000U

volatile uint8_t interlock_test_initialized;
volatile GPIO_PinState interlock_fault_x1_awake_level;
volatile uint8_t interlock_fault_x1_awake_asserted;
volatile uint8_t interlock_ena_x_mcu_command_high;
volatile uint32_t interlock_wake_tick_ms;
volatile uint32_t interlock_fault_x1_awake_tick_ms;
volatile uint32_t interlock_last_ena_x_transition_tick_ms;
volatile uint32_t interlock_ena_x_high_cycle_count;

static void Interlock_Diagnostic_ForceSafeState(void);

/**
 * @brief  Establish safe driver state and wake U4/U5 for an interlock test.
 * @note   No SPI transfer, register write, STEP pulse, or MOTION_OK action is
 *         performed. MOTION_OK must remain physically disconnected/LOW.
 */
void Interlock_Diagnostic_Initialize(void)
{
  interlock_test_initialized = 0U;
  interlock_fault_x1_awake_level = GPIO_PIN_RESET;
  interlock_fault_x1_awake_asserted = 1U;
  interlock_ena_x_mcu_command_high = 0U;
  interlock_wake_tick_ms = 0U;
  interlock_fault_x1_awake_tick_ms = 0U;
  interlock_last_ena_x_transition_tick_ms = 0U;
  interlock_ena_x_high_cycle_count = 0U;

  Interlock_Diagnostic_ForceSafeState();

  /* Exceeds DRV8461 tSLEEP, restoring its safe reset-default configuration. */
  HAL_Delay(INTERLOCK_SLEEP_RESET_MS);

  /* U4/U5 are awake for this observation; all MCU enable commands remain LOW. */
  HAL_GPIO_WritePin(SLEEP_X_GPIO_Port, SLEEP_X_Pin, GPIO_PIN_SET);
  interlock_wake_tick_ms = HAL_GetTick();
  HAL_Delay(INTERLOCK_AWAKE_SETTLE_MS);

  /* Do not pulse PF9 if U4 still reports an awake fault condition. */
  interlock_fault_x1_awake_tick_ms = HAL_GetTick();
  interlock_fault_x1_awake_level = HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin);
  interlock_fault_x1_awake_asserted =
      (interlock_fault_x1_awake_level == GPIO_PIN_RESET) ? 1U : 0U;
  interlock_test_initialized = 1U;
}

/**
 * @brief  Make PF9 and D5 visibly HIGH together, then LOW together.
 * @note   This deliberately tests only the MCU side of the U9 AND gate. With
 *         MOTION_OK physically LOW, U9's driver-enable output must stay LOW.
 *         The DRV8461 remains at its reset default CTRL1.EN_OUT=0; no output
 *         enable, SPI write, or STEP command is issued by this firmware.
 */
void Interlock_Diagnostic_PulseXEnable(void)
{
  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_SET);
  interlock_ena_x_mcu_command_high = 1U;
  interlock_last_ena_x_transition_tick_ms = HAL_GetTick();
  interlock_ena_x_high_cycle_count++;
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
  HAL_Delay(INTERLOCK_ENABLE_PHASE_MS);

  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_RESET);
  interlock_ena_x_mcu_command_high = 0U;
  interlock_last_ena_x_transition_tick_ms = HAL_GetTick();
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
  HAL_Delay(INTERLOCK_ENABLE_PHASE_MS);
}

/**
 * @brief  Keep PF9 LOW and show that U4 remained faulted after the settle wait.
 */
void Interlock_Diagnostic_IndicateAwakeFault(void)
{
  Interlock_Diagnostic_ForceSafeState();
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
  HAL_Delay(INTERLOCK_FAULT_ON_MS);
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
  HAL_Delay(INTERLOCK_FAULT_OFF_MS);
}

static void Interlock_Diagnostic_ForceSafeState(void)
{
  HAL_GPIO_WritePin(CS_X1_GPIO_Port, CS_X1_Pin | CS_X2_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Y_GPIO_Port, CS_Y_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Z_GPIO_Port, CS_Z_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(SLEEP_X_GPIO_Port, SLEEP_X_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Y_MCU_GPIO_Port, ENA_Y_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Z_MCU_GPIO_Port, ENA_Z_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
}
