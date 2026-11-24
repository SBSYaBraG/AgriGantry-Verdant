/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : motor_test.c
 * @brief          : U4 driver preparation and bounded bench motion profiles.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "motor_test.h"
#include "encoder_diag.h"
#include "spi.h"
#include "tim.h"

/* DRV8461SPWPR register addresses. CTRL13 is 0x10, not 0x1A. */
#define DRV8461_REG_FAULT                 0x00U
#define DRV8461_REG_DIAG2                 0x02U
#define DRV8461_REG_CTRL1                 0x04U
#define DRV8461_REG_CTRL2                 0x05U
#define DRV8461_REG_CTRL11                0x0EU
#define DRV8461_REG_CTRL13                0x10U

#define DRV8461_CTRL1_DISABLED            0x0FU
#define DRV8461_CTRL1_ENABLE_OUTPUTS      0x8FU
#define DRV8461_CTRL2_RESET_VALUE         0x06U
#define DRV8461_CTRL13_INTERNAL_VREF      0x12U
#define DRV8461_CTRL11_030A_TRQ_DAC       0x19U
#define DRV8461_CTRL1_EN_OUT_MASK         0x80U
#define DRV8461_STATUS_HEADER_MASK        0xC0U
#define DRV8461_STATUS_HEADER_VALUE       0xC0U
#define DRV8461_STATUS_FAULT_MASK         0x3FU
#define DRV8461_DIAG2_OTW_MASK            0x40U
#define DRV8461_DIAG2_OTS_MASK            0x20U
#define DRV8461_SPI_TIMEOUT_MS            10U
#define MOTOR_TEST_SLEEP_RESET_MS         1U
#define MOTOR_TEST_AWAKE_SETTLE_MS        30U
#define MOTOR_TEST_LED_FLASH_MS           80U
#define MOTOR_TEST_LED_PAUSE_MS           2000U
#define MOTOR_TEST_HOLD_DURATION_MS       2000U
#define MOTOR_TEST_HOLD_SAMPLE_PERIOD_MS  100U
#define MOTOR_TEST_SLOW_STEP_PULSE_COUNT   16U
#define MOTOR_TEST_SLOW_STEP_HIGH_MS       2U
#define MOTOR_TEST_SLOW_STEP_LOW_MS        18U
#define MOTOR_TEST_ONE_REV_MICROSTEPS      3200U
#define MOTOR_TEST_ONE_REV_HIGH_MS         2U
#define MOTOR_TEST_ONE_REV_DIAG_INTERVAL   100U
#define MOTOR_TEST_ONE_REV_ACCEL_STEPS      400U
#define MOTOR_TEST_ONE_REV_CRUISE_STEPS    2400U
#define MOTOR_TEST_ONE_REV_DECEL_STEPS      400U
#define MOTOR_TEST_ONE_REV_LOW_START_MS      10U
#define MOTOR_TEST_ONE_REV_LOW_CRUISE_MS      1U
#define MOTOR_TEST_ENCODER_SETTLE_MS        100U
#define MOTOR_TEST_ENCODER_EXPECTED_COUNTS 4000L
#define MOTOR_TEST_ENCODER_TOLERANCE_COUNTS  200L

_Static_assert((MOTOR_TEST_ONE_REV_ACCEL_STEPS +
                MOTOR_TEST_ONE_REV_CRUISE_STEPS +
                MOTOR_TEST_ONE_REV_DECEL_STEPS) == MOTOR_TEST_ONE_REV_MICROSTEPS,
               "Test 10 trajectory phases must equal one commanded revolution");

/* Failure-mask bits retained for a Hot Plug SRAM read if preparation fails. */
#define MOTOR_TEST_FAIL_SPI_INIT            (1UL << 0)
#define MOTOR_TEST_FAIL_NFAULT              (1UL << 1)
#define MOTOR_TEST_FAIL_U4_FAULT             (1UL << 2)
#define MOTOR_TEST_FAIL_U5_FAULT             (1UL << 3)
#define MOTOR_TEST_FAIL_U4_EN_OUT            (1UL << 4)
#define MOTOR_TEST_FAIL_U5_EN_OUT            (1UL << 5)
#define MOTOR_TEST_FAIL_SPI_FRAME            (1UL << 6)
#define MOTOR_TEST_FAIL_CONFIG_READBACK      (1UL << 7)

volatile HAL_StatusTypeDef motor_test_spi_init_status;
volatile HAL_StatusTypeDef motor_test_u4_fault_read_status;
volatile HAL_StatusTypeDef motor_test_u5_fault_read_status;
volatile HAL_StatusTypeDef motor_test_u4_ctrl1_read_status;
volatile HAL_StatusTypeDef motor_test_u5_ctrl1_read_status;
volatile HAL_StatusTypeDef motor_test_u4_ctrl1_disable_write_status;
volatile HAL_StatusTypeDef motor_test_u4_ctrl1_disabled_read_status;
volatile HAL_StatusTypeDef motor_test_u4_ctrl13_write_status;
volatile HAL_StatusTypeDef motor_test_u4_ctrl13_read_status;
volatile HAL_StatusTypeDef motor_test_u4_ctrl11_write_status;
volatile HAL_StatusTypeDef motor_test_u4_ctrl11_read_status;
volatile HAL_StatusTypeDef motor_test_u4_ctrl2_read_status;
volatile HAL_StatusTypeDef motor_test_u5_ctrl1_final_read_status;

volatile uint16_t motor_test_u4_fault_raw_frame;
volatile uint16_t motor_test_u5_fault_raw_frame;
volatile uint16_t motor_test_u4_ctrl1_raw_frame;
volatile uint16_t motor_test_u5_ctrl1_raw_frame;
volatile uint16_t motor_test_u4_ctrl1_disable_raw_frame;
volatile uint16_t motor_test_u4_ctrl1_disabled_raw_frame;
volatile uint16_t motor_test_u4_ctrl13_write_raw_frame;
volatile uint16_t motor_test_u4_ctrl13_raw_frame;
volatile uint16_t motor_test_u4_ctrl11_write_raw_frame;
volatile uint16_t motor_test_u4_ctrl11_raw_frame;
volatile uint16_t motor_test_u4_ctrl2_raw_frame;
volatile uint16_t motor_test_u5_ctrl1_final_raw_frame;

volatile uint8_t motor_test_u4_fault;
volatile uint8_t motor_test_u5_fault;
volatile uint8_t motor_test_u4_ctrl1_initial;
volatile uint8_t motor_test_u5_ctrl1_initial;
volatile uint8_t motor_test_u4_ctrl1_disabled;
volatile uint8_t motor_test_u4_ctrl13_readback;
volatile uint8_t motor_test_u4_ctrl11_readback;
volatile uint8_t motor_test_u4_ctrl2_readback;
volatile uint8_t motor_test_u5_ctrl1_final;
volatile GPIO_PinState motor_test_fault_x1_awake_level;
volatile uint8_t motor_test_fault_x1_awake_asserted;
volatile uint32_t motor_test_failure_mask;
volatile uint8_t motor_test_preparation_passed;
volatile uint32_t motor_test_wake_tick_ms;
volatile uint32_t motor_test_fault_sample_tick_ms;

/*
 * These BSS variables are deliberately initialized to zero on every reset.
 * The exact magic pair must be written through SWD after boot to request one
 * hold.  The request is consumed before any motor-control action is taken.
 */
/*
 * STM32CubeProgrammer's Fill Memory operation writes on 8-byte boundaries.
 * Keep each independently writable arm word on its own such boundary; do not
 * rely on the programmer silently rounding an unaligned address down.
 */
volatile uint32_t motor_test_hold_arm_key __attribute__((aligned(8)));
volatile uint32_t motor_test_hold_arm_key_complement __attribute__((aligned(8)));
volatile uint8_t motor_test_hold_state;
volatile HAL_StatusTypeDef motor_test_hold_status;
volatile HAL_StatusTypeDef motor_test_hold_u5_ctrl1_read_status;
volatile HAL_StatusTypeDef motor_test_hold_ctrl1_enable_write_status;
volatile HAL_StatusTypeDef motor_test_hold_ctrl1_enable_read_status;
volatile HAL_StatusTypeDef motor_test_hold_fault_read_status;
volatile uint16_t motor_test_hold_u5_ctrl1_raw_frame;
volatile uint16_t motor_test_hold_ctrl1_enable_write_raw_frame;
volatile uint16_t motor_test_hold_ctrl1_enable_read_raw_frame;
volatile uint16_t motor_test_hold_fault_raw_frame;
volatile uint8_t motor_test_hold_u5_ctrl1_before_enable;
volatile uint8_t motor_test_hold_ctrl1_enabled_readback;
volatile uint8_t motor_test_hold_fault;
volatile GPIO_PinState motor_test_hold_nfault_at_enable;
volatile GPIO_PinState motor_test_hold_nfault_at_shutdown;
volatile uint32_t motor_test_hold_start_tick_ms;
volatile uint32_t motor_test_hold_end_tick_ms;
volatile uint32_t motor_test_hold_fault_sample_count;

volatile HAL_StatusTypeDef motor_test_slow_step_status;
volatile uint8_t motor_test_slow_step_pulses_issued;
volatile GPIO_PinState motor_test_slow_step_nfault_at_shutdown;
volatile uint32_t motor_test_slow_step_start_tick_ms;
volatile uint32_t motor_test_slow_step_end_tick_ms;

volatile HAL_StatusTypeDef motor_test_one_rev_status;
volatile HAL_StatusTypeDef motor_test_one_rev_fault_read_status;
volatile HAL_StatusTypeDef motor_test_one_rev_diag2_read_status;
volatile uint16_t motor_test_one_rev_pulses_issued;
volatile uint16_t motor_test_one_rev_diagnostic_sample_count;
volatile uint16_t motor_test_one_rev_fault_raw_frame;
volatile uint16_t motor_test_one_rev_diag2_raw_frame;
volatile uint8_t motor_test_one_rev_fault;
volatile uint8_t motor_test_one_rev_diag2;
volatile uint8_t motor_test_one_rev_otw_seen;
volatile uint8_t motor_test_one_rev_ots_seen;
volatile GPIO_PinState motor_test_one_rev_nfault_at_shutdown;
volatile uint32_t motor_test_one_rev_start_tick_ms;
volatile uint32_t motor_test_one_rev_end_tick_ms;

volatile uint8_t motor_test_encoder_ready_at_arm;
volatile uint8_t motor_test_encoder_window_started;
volatile uint32_t motor_test_encoder_raw_before_enable;
volatile uint32_t motor_test_encoder_raw_start;
volatile uint32_t motor_test_encoder_raw_after_accel;
volatile uint32_t motor_test_encoder_raw_after_cruise;
volatile uint32_t motor_test_encoder_raw_end;
volatile int32_t motor_test_encoder_delta_accel;
volatile int32_t motor_test_encoder_delta_cruise;
volatile int32_t motor_test_encoder_delta_decel;
volatile int32_t motor_test_encoder_delta_total;
volatile uint32_t motor_test_encoder_index_start;
volatile uint32_t motor_test_encoder_index_end;
volatile uint32_t motor_test_encoder_index_delta;
volatile uint8_t motor_test_encoder_count_within_tolerance;
volatile uint8_t motor_test_encoder_index_seen;
volatile uint32_t motor_test_encoder_accel_end_tick_ms;
volatile uint32_t motor_test_encoder_cruise_end_tick_ms;

static void Motor_Test_ClearResults(void);
static void Motor_Test_ClearHoldResults(void);
static void Motor_Test_ClearSlowStepResults(void);
static void Motor_Test_ClearOneRevolutionResults(void);
static void Motor_Test_ForceSafeState(void);
static HAL_StatusTypeDef Motor_Test_ConfigureDisabled(void);
static HAL_StatusTypeDef WakeXDisabled(void);
static HAL_StatusTypeDef DRV8461_ReadRegister(GPIO_TypeDef *cs_port,
                                               uint16_t cs_pin,
                                               uint8_t address,
                                               volatile uint16_t *raw_frame,
                                               volatile uint8_t *register_value);
static HAL_StatusTypeDef DRV8461_WriteRegister(GPIO_TypeDef *cs_port,
                                                uint16_t cs_pin,
                                                uint8_t address,
                                                uint8_t value,
                                                volatile uint16_t *raw_frame);
static uint8_t DRV8461_FrameIsHealthy(uint16_t raw_frame);
static bool DRV8461_FrameIsValid(uint16_t raw_frame);
static MotorMoveResult Motor_Test_EnableU4Outputs(bool (*stop_requested)(void));
static uint32_t Motor_Test_OneRevLowTimeMs(uint16_t pulse_index);
static void Motor_Test_DecodeEncoderEnd(bool completed);
static uint8_t Motor_Test_EncoderCountIsWithinTolerance(int32_t count);
static void Motor_Test_IndicateEncoderInconclusive(void);

/**
 * @brief  Validate and program U4 while all H-bridges are disabled at boot.
 * @note   This Test 7 boot routine never sets ENA_X_MCU HIGH, never writes
 *         CTRL1.EN_OUT=1, and ends in sleep awaiting the post-boot arm pair.
 */
void Motor_Test_Prepare(void)
{
  /* A reset can never inherit an SWD arm request from a prior test. */
  motor_test_hold_arm_key = 0U;
  motor_test_hold_arm_key_complement = 0U;
  Motor_Test_ClearHoldResults();
  Motor_Test_ClearSlowStepResults();
  Motor_Test_ClearOneRevolutionResults();

  if (Motor_Test_ConfigureDisabled() == HAL_OK)
  {
    motor_test_preparation_passed = 1U;
    motor_test_hold_state = MOTOR_TEST_HOLD_WAITING;
  }
  else
  {
    motor_test_hold_state = MOTOR_TEST_HOLD_PREPARATION_FAIL;
  }

  /* A completed preparation has no persistent motor-drive state. */
  Motor_Test_ForceSafeState();
}

/**
 * @brief  Process an explicit post-boot arm request, then perform one Test 10 move.
 * @note   There is no code path from reset directly to output enable.
 */
void Motor_Test_Service(void)
{
  if (motor_test_hold_state == MOTOR_TEST_HOLD_WAITING)
  {
    if ((motor_test_hold_arm_key == MOTOR_TEST_HOLD_ARM_MAGIC) &&
        (motor_test_hold_arm_key_complement == MOTOR_TEST_HOLD_ARM_MAGIC_COMPLEMENT))
    {
      /* Consume the request before waking/reconfiguring the driver. */
      motor_test_hold_arm_key = 0U;
      motor_test_hold_arm_key_complement = 0U;
      motor_test_hold_state = MOTOR_TEST_HOLD_ACTIVE;
      motor_test_one_rev_status = Motor_Test_RunOneRevolution();
      motor_test_hold_state = (motor_test_one_rev_status == HAL_OK) ?
                              MOTOR_TEST_HOLD_PASS : MOTOR_TEST_HOLD_FAIL;
    }
    else
    {
      Motor_Test_IndicateWaiting();
    }
  }
  else if (motor_test_hold_state == MOTOR_TEST_HOLD_PASS)
  {
    if (motor_test_encoder_count_within_tolerance != 0U)
    {
      Motor_Test_IndicatePass();
    }
    else
    {
      /* Motion completed safely, but encoder evidence is absent or out of range. */
      Motor_Test_IndicateEncoderInconclusive();
    }
  }
  else
  {
    Motor_Test_IndicateFailure();
  }
}

/**
 * @brief  One short flash: configuration passed and the MCU is safely waiting
 *         for the deliberate SWD arm pair.
 */
void Motor_Test_IndicateWaiting(void)
{
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
  HAL_Delay(200U);
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
  HAL_Delay(MOTOR_TEST_LED_PAUSE_MS);
}

/**
 * @brief  Five rapid flashes: driver checks and the commanded one-revolution
 *         trajectory completed, with encoder count within the expected range.
 * @note   This confirms firmware checks and encoder scaling only; it cannot
 *         electrically read the external ENA_X or MOTION_OK nets.
 */
void Motor_Test_IndicatePass(void)
{
  uint8_t flash_count;

  for (flash_count = 0U; flash_count < 5U; ++flash_count)
  {
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
    HAL_Delay(MOTOR_TEST_LED_FLASH_MS);
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
    HAL_Delay(MOTOR_TEST_LED_FLASH_MS);
  }

  HAL_Delay(MOTOR_TEST_LED_PAUSE_MS);
}

/**
  * @brief  Two medium flashes: motor diagnostics stayed clean, but the
  *         measured A/B count was not within the expected one-revolution range.
  */
static void Motor_Test_IndicateEncoderInconclusive(void)
{
  uint8_t flash_count;

  for (flash_count = 0U; flash_count < 2U; ++flash_count)
  {
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
    HAL_Delay(300U);
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
    HAL_Delay(200U);
  }

  HAL_Delay(MOTOR_TEST_LED_PAUSE_MS);
}

/**
 * @brief  One long flash identifies a preparation/hold failure; outputs stay off.
 */
void Motor_Test_IndicateFailure(void)
{
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
  HAL_Delay(1000U);
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
  HAL_Delay(MOTOR_TEST_LED_PAUSE_MS);
}

/**
 * @brief  Preserved Test 6 hold implementation; unreferenced by Test 8.
 * @note   The linked Test 8 image invokes Motor_Test_RunOneRevolution() instead.
 */
HAL_StatusTypeDef Motor_Test_RunHoldingTorque(void)
{
  uint32_t elapsed_ms;

  Motor_Test_ClearHoldResults();
  if (Motor_Test_ConfigureDisabled() != HAL_OK)
  {
    Motor_Test_ForceSafeState();
    return HAL_ERROR;
  }

  if (Motor_Test_EnableU4Outputs(NULL) != MOTOR_MOVE_DONE)
  {
    Motor_Test_ForceSafeState();
    return HAL_ERROR;
  }

  motor_test_hold_start_tick_ms = HAL_GetTick();
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);

  /* Two seconds of holding torque; no STEP edge is generated. */
  for (elapsed_ms = 0U; elapsed_ms < MOTOR_TEST_HOLD_DURATION_MS; elapsed_ms += 10U)
  {
    if (HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin) == GPIO_PIN_RESET)
    {
      motor_test_hold_nfault_at_shutdown = GPIO_PIN_RESET;
      Motor_Test_ForceSafeState();
      motor_test_hold_end_tick_ms = HAL_GetTick();
      return HAL_ERROR;
    }

    if ((elapsed_ms % MOTOR_TEST_HOLD_SAMPLE_PERIOD_MS) == 0U)
    {
      motor_test_hold_fault_read_status =
          DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_FAULT,
                               &motor_test_hold_fault_raw_frame,
                               &motor_test_hold_fault);
      motor_test_hold_fault_sample_count++;
      if ((motor_test_hold_fault_read_status != HAL_OK) ||
          (DRV8461_FrameIsHealthy(motor_test_hold_fault_raw_frame) == 0U) ||
          (motor_test_hold_fault != 0U))
      {
        motor_test_hold_nfault_at_shutdown =
            HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin);
        Motor_Test_ForceSafeState();
        motor_test_hold_end_tick_ms = HAL_GetTick();
        return HAL_ERROR;
      }
    }
    HAL_Delay(10U);
  }

  motor_test_hold_nfault_at_shutdown =
      HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin);
  Motor_Test_ForceSafeState();
  motor_test_hold_end_tick_ms = HAL_GetTick();
  return HAL_OK;
}

/**
 * @brief  Preserved Test 7: 16-pulse low-current slow STEP test.
 * @note   At the verified reset 1/16 microstep setting, 16 rising edges are
 *         one 1.8-degree motor full-step equivalent. DIR_X is held LOW.
 */
HAL_StatusTypeDef Motor_Test_RunSlowStep(void)
{
  uint8_t pulse_count;

  Motor_Test_ClearHoldResults();
  Motor_Test_ClearSlowStepResults();

  if (Motor_Test_ConfigureDisabled() != HAL_OK)
  {
    Motor_Test_ForceSafeState();
    return HAL_ERROR;
  }

  HAL_GPIO_WritePin(DIR_X_GPIO_Port, DIR_X_Pin, GPIO_PIN_RESET);
  HAL_Delay(5U);
  if (Motor_Test_EnableU4Outputs(NULL) != MOTOR_MOVE_DONE)
  {
    Motor_Test_ForceSafeState();
    return HAL_ERROR;
  }

  motor_test_slow_step_start_tick_ms = HAL_GetTick();
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);

  for (pulse_count = 0U; pulse_count < MOTOR_TEST_SLOW_STEP_PULSE_COUNT; ++pulse_count)
  {
    if (HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin) == GPIO_PIN_RESET)
    {
      motor_test_slow_step_nfault_at_shutdown = GPIO_PIN_RESET;
      Motor_Test_ForceSafeState();
      motor_test_slow_step_end_tick_ms = HAL_GetTick();
      return HAL_ERROR;
    }
    HAL_GPIO_WritePin(STEP_X_GPIO_Port, STEP_X_Pin, GPIO_PIN_SET);
    HAL_Delay(MOTOR_TEST_SLOW_STEP_HIGH_MS);
    if (HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin) == GPIO_PIN_RESET)
    {
      motor_test_slow_step_nfault_at_shutdown = GPIO_PIN_RESET;
      Motor_Test_ForceSafeState();
      motor_test_slow_step_end_tick_ms = HAL_GetTick();
      return HAL_ERROR;
    }
    HAL_GPIO_WritePin(STEP_X_GPIO_Port, STEP_X_Pin, GPIO_PIN_RESET);
    HAL_Delay(MOTOR_TEST_SLOW_STEP_LOW_MS);
    motor_test_slow_step_pulses_issued++;
  }

  motor_test_slow_step_nfault_at_shutdown =
      HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin);
  if (motor_test_slow_step_nfault_at_shutdown == GPIO_PIN_RESET)
  {
    Motor_Test_ForceSafeState();
    motor_test_slow_step_end_tick_ms = HAL_GetTick();
    return HAL_ERROR;
  }
  Motor_Test_ForceSafeState();
  motor_test_slow_step_end_tick_ms = HAL_GetTick();
  return HAL_OK;
}

/**
 * @brief  Test 10: explicitly armed one-revolution trapezoid trajectory with
 *         U13/TIM5 encoder capture.
 * @note   3200 rising edges at 1/16 microstep equal one 200-full-step motor
 *         revolution.  Pulse low time ramps 10 ms -> 1 ms -> 10 ms; high time
 *         remains 2 ms.  DIR_X remains LOW throughout.
 */
HAL_StatusTypeDef Motor_Test_RunOneRevolution(void)
{
  return Motor_Test_RunSupervisedRevolution(NULL) == MOTOR_MOVE_DONE ? HAL_OK : HAL_ERROR;
}

static MotorMoveResult MotionGuard(bool (*stop_requested)(void))
{
  motor_test_one_rev_nfault_at_shutdown = HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin);
  if (motor_test_one_rev_nfault_at_shutdown == GPIO_PIN_RESET)
    return MOTOR_MOVE_X1_NFAULT;
  if (HAL_GPIO_ReadPin(FAULT_X2_GPIO_Port, FAULT_X2_Pin) == GPIO_PIN_RESET)
    return MOTOR_MOVE_X2_NFAULT;
  if (stop_requested != NULL && stop_requested())
    return MOTOR_MOVE_STOPPED;
  return MOTOR_MOVE_DONE;
}

static MotorMoveResult ReadMotionDiagnostics(void)
{
  motor_test_one_rev_fault_read_status =
      DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_FAULT,
                           &motor_test_one_rev_fault_raw_frame, &motor_test_one_rev_fault);
  motor_test_one_rev_diag2_read_status =
      DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_DIAG2,
                           &motor_test_one_rev_diag2_raw_frame, &motor_test_one_rev_diag2);
  ++motor_test_one_rev_diagnostic_sample_count;
  if (motor_test_one_rev_fault_read_status != HAL_OK || motor_test_one_rev_diag2_read_status != HAL_OK ||
      !DRV8461_FrameIsValid(motor_test_one_rev_fault_raw_frame) ||
      !DRV8461_FrameIsValid(motor_test_one_rev_diag2_raw_frame))
    return MOTOR_MOVE_SPI_ERROR;

  motor_test_one_rev_otw_seen |= (motor_test_one_rev_diag2 & DRV8461_DIAG2_OTW_MASK) != 0U;
  motor_test_one_rev_ots_seen |= (motor_test_one_rev_diag2 & DRV8461_DIAG2_OTS_MASK) != 0U;
  if (motor_test_one_rev_otw_seen || motor_test_one_rev_ots_seen)
    return MOTOR_MOVE_THERMAL;
  if (motor_test_one_rev_fault != 0U || !DRV8461_FrameIsHealthy(motor_test_one_rev_fault_raw_frame) ||
      !DRV8461_FrameIsHealthy(motor_test_one_rev_diag2_raw_frame))
    return MOTOR_MOVE_DRIVER_FAULT;
  return MOTOR_MOVE_DONE;
}

/* Both chips are on GPIOE and share STEP, DIR, nSLEEP and the U9 enable gate. */
static const uint16_t x_chip_select[] = {CS_X1_Pin, CS_X2_Pin};
MotorXDriverReport motor_pair_driver[2];

static bool PairRead(unsigned chip, uint8_t reg, uint8_t *value)
{
  MotorXDriverReport *d = &motor_pair_driver[chip];
  d->last_reg = reg;
  d->last_hal = DRV8461_ReadRegister(GPIOE, x_chip_select[chip], reg, &d->last_frame, value);
  return d->last_hal == HAL_OK && DRV8461_FrameIsHealthy(d->last_frame);
}

static bool PairWriteVerify(unsigned chip, uint8_t reg, uint8_t value)
{
  MotorXDriverReport *d = &motor_pair_driver[chip];
  d->last_reg = reg;
  d->last_hal = DRV8461_WriteRegister(GPIOE, x_chip_select[chip], reg, value, &d->last_frame);
  if (d->last_hal != HAL_OK || !DRV8461_FrameIsHealthy(d->last_frame))
    return false;
  uint8_t readback;
  bool healthy = PairRead(chip, reg, &readback);
  if (reg == DRV8461_REG_CTRL1 && healthy)
    d->ctrl1 = readback;
  return healthy && readback == value;
}

static MotorMoveResult PairDiagnostics(bool (*stop_requested)(void))
{
  for (unsigned chip = 0; chip < 2U; ++chip)
  {
    MotorMoveResult guard = MotionGuard(stop_requested);
    if (guard != MOTOR_MOVE_DONE) return guard;
    MotorXDriverReport *d = &motor_pair_driver[chip];
    uint8_t fault, diag2;
    bool healthy = PairRead(chip, DRV8461_REG_FAULT, &fault);
    d->fault_frame = d->last_frame;
    if (d->last_hal != HAL_OK || !DRV8461_FrameIsValid(d->last_frame))
      return MOTOR_MOVE_SPI_ERROR;
    bool diag_healthy = PairRead(chip, DRV8461_REG_DIAG2, &diag2);
    d->diag2_frame = d->last_frame;
    if (d->last_hal != HAL_OK || !DRV8461_FrameIsValid(d->last_frame))
      return MOTOR_MOVE_SPI_ERROR;
    ++d->samples;
    if ((diag2 & (DRV8461_DIAG2_OTW_MASK | DRV8461_DIAG2_OTS_MASK)) != 0U)
      return MOTOR_MOVE_THERMAL;
    if (!healthy || !diag_healthy || fault != 0U || (diag2 & 0x03U) != 0U)
      return MOTOR_MOVE_DRIVER_FAULT; /* DIAG2 bits 1:0 are OL_B and OL_A. */
  }
  return MOTOR_MOVE_DONE;
}

static MotorMoveResult ConfigurePair(bool (*stop_requested)(void))
{
  if (WakeXDisabled() != HAL_OK) return MOTOR_MOVE_PREPARATION_FAILED;
  MotorMoveResult result = PairDiagnostics(stop_requested);
  if (result != MOTOR_MOVE_DONE) return result;
  for (unsigned chip = 0; chip < 2U; ++chip)
  {
    uint8_t value;
    if (!PairRead(chip, DRV8461_REG_CTRL1, &value) || value != DRV8461_CTRL1_DISABLED ||
        !PairWriteVerify(chip, DRV8461_REG_CTRL1, DRV8461_CTRL1_DISABLED) ||
        !PairWriteVerify(chip, DRV8461_REG_CTRL13, DRV8461_CTRL13_INTERNAL_VREF) ||
        !PairWriteVerify(chip, DRV8461_REG_CTRL11, DRV8461_CTRL11_030A_TRQ_DAC) ||
        !PairRead(chip, DRV8461_REG_CTRL2, &value) || value != DRV8461_CTRL2_RESET_VALUE)
      return MOTOR_MOVE_PREPARATION_FAILED;
    motor_pair_driver[chip].configured = 1U;
  }
  return MotionGuard(stop_requested);
}

static MotorMoveResult EnablePair(bool (*stop_requested)(void))
{
  /* U9 is held off through both EN_OUT writes and independent readbacks. */
  for (unsigned chip = 0; chip < 2U; ++chip)
  {
    MotorMoveResult result = MotionGuard(stop_requested);
    if (result != MOTOR_MOVE_DONE) return result;
    if (!PairWriteVerify(chip, DRV8461_REG_CTRL1, DRV8461_CTRL1_ENABLE_OUTPUTS))
      return MOTOR_MOVE_ENABLE_FAILED;
  }
  MotorMoveResult result = MotionGuard(stop_requested);
  if (result != MOTOR_MOVE_DONE) return result;
  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_SET);
  return MotionGuard(stop_requested);
}

static MotorMoveResult RunTrajectory(bool paired, bool (*stop_requested)(void))
{
  motor_test_encoder_ready_at_arm =
      (encoder_diag_initialized != 0U && encoder_diag_timer_start_status == HAL_OK);
  if (!motor_test_encoder_ready_at_arm)
    return MOTOR_MOVE_ENCODER_UNAVAILABLE;
  if (stop_requested != NULL && stop_requested())
    return MOTOR_MOVE_STOPPED;
  MotorMoveResult result = MOTOR_MOVE_DONE;
  if (paired)
    result = ConfigurePair(stop_requested);
  else if (Motor_Test_ConfigureDisabled() != HAL_OK)
    result = MOTOR_MOVE_PREPARATION_FAILED;
  if (result != MOTOR_MOVE_DONE) return result;

  HAL_GPIO_WritePin(DIR_X_GPIO_Port, DIR_X_Pin, GPIO_PIN_RESET);
  HAL_Delay(5U);
  result = paired ? PairDiagnostics(stop_requested) : ReadMotionDiagnostics();
  if (result != MOTOR_MOVE_DONE)
    return result;
  result = MotionGuard(stop_requested);
  if (result != MOTOR_MOVE_DONE)
    return result;
  motor_test_encoder_raw_before_enable = __HAL_TIM_GET_COUNTER(&htim5);
  result = paired ? EnablePair(stop_requested) : Motor_Test_EnableU4Outputs(stop_requested);
  if (result != MOTOR_MOVE_DONE)
    return result;

  /* Keep sampling during alignment. Exclude it from the commanded distance. */
  uint32_t enabled_at = HAL_GetTick();
  do
  {
    result = MotionGuard(stop_requested);
    if (result != MOTOR_MOVE_DONE)
      return result;
    HAL_Delay(1U);
  } while ((uint32_t)(HAL_GetTick() - enabled_at) < MOTOR_TEST_ENCODER_SETTLE_MS);

  motor_test_one_rev_start_tick_ms = HAL_GetTick();
  motor_test_encoder_raw_start = __HAL_TIM_GET_COUNTER(&htim5);
  motor_test_encoder_index_start = encoder_diag_index_event_count;
  motor_test_encoder_window_started = 1U;
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);

  for (uint16_t pulse = 0; pulse < MOTOR_TEST_ONE_REV_MICROSTEPS; ++pulse)
  {
    result = MotionGuard(stop_requested);
    if (result != MOTOR_MOVE_DONE)
      return result;
    HAL_GPIO_WritePin(STEP_X_GPIO_Port, STEP_X_Pin, GPIO_PIN_SET);
    ++motor_test_one_rev_pulses_issued; /* Count the actual rising edge even on abort. */
    HAL_Delay(MOTOR_TEST_ONE_REV_HIGH_MS);
    HAL_GPIO_WritePin(STEP_X_GPIO_Port, STEP_X_Pin, GPIO_PIN_RESET);
    result = MotionGuard(stop_requested);
    if (result != MOTOR_MOVE_DONE)
      return result;
    HAL_Delay(Motor_Test_OneRevLowTimeMs(pulse));

    if (motor_test_one_rev_pulses_issued == MOTOR_TEST_ONE_REV_ACCEL_STEPS)
    {
      motor_test_encoder_raw_after_accel = __HAL_TIM_GET_COUNTER(&htim5);
      motor_test_encoder_accel_end_tick_ms = HAL_GetTick();
    }
    else if (motor_test_one_rev_pulses_issued == MOTOR_TEST_ONE_REV_ACCEL_STEPS + MOTOR_TEST_ONE_REV_CRUISE_STEPS)
    {
      motor_test_encoder_raw_after_cruise = __HAL_TIM_GET_COUNTER(&htim5);
      motor_test_encoder_cruise_end_tick_ms = HAL_GetTick();
    }
    if (motor_test_one_rev_pulses_issued % MOTOR_TEST_ONE_REV_DIAG_INTERVAL == 0U)
    {
      result = MotionGuard(stop_requested);
      if (result != MOTOR_MOVE_DONE)
        return result;
      result = paired ? PairDiagnostics(stop_requested) : ReadMotionDiagnostics();
      if (result != MOTOR_MOVE_DONE)
        return result;
    }
  }
  return MotionGuard(stop_requested);
}

static MotorMoveResult RunSupervisedRevolution(bool paired, bool (*stop_requested)(void))
{
  Motor_Test_ClearHoldResults();
  Motor_Test_ClearOneRevolutionResults();
  MotorMoveResult result = RunTrajectory(paired, stop_requested);
  if (motor_test_encoder_window_started)
  {
    /* Take the two end readings before releasing holding torque. */
    motor_test_encoder_raw_end = __HAL_TIM_GET_COUNTER(&htim5);
    motor_test_encoder_index_end = encoder_diag_index_event_count;
  }
  Motor_Test_ForceSafeState();
  motor_test_one_rev_end_tick_ms = HAL_GetTick();
  motor_test_one_rev_status = (result == MOTOR_MOVE_DONE) ? HAL_OK : HAL_ERROR;
  if (motor_test_encoder_window_started)
    Motor_Test_DecodeEncoderEnd(result == MOTOR_MOVE_DONE);
  return result;
}

MotorMoveResult Motor_Test_RunSupervisedRevolution(bool (*stop_requested)(void))
{
  return RunSupervisedRevolution(false, stop_requested);
}

MotorMoveResult Motor_Test_RunPairedRevolution(bool (*stop_requested)(void))
{
  for (unsigned chip = 0; chip < 2U; ++chip)
    motor_pair_driver[chip] = (MotorXDriverReport){0};
  return RunSupervisedRevolution(true, stop_requested);
}

static void Motor_Test_ClearResults(void)
{
  motor_test_spi_init_status = HAL_ERROR;
  motor_test_u4_fault_read_status = HAL_ERROR;
  motor_test_u5_fault_read_status = HAL_ERROR;
  motor_test_u4_ctrl1_read_status = HAL_ERROR;
  motor_test_u5_ctrl1_read_status = HAL_ERROR;
  motor_test_u4_ctrl1_disable_write_status = HAL_ERROR;
  motor_test_u4_ctrl1_disabled_read_status = HAL_ERROR;
  motor_test_u4_ctrl13_write_status = HAL_ERROR;
  motor_test_u4_ctrl13_read_status = HAL_ERROR;
  motor_test_u4_ctrl11_write_status = HAL_ERROR;
  motor_test_u4_ctrl11_read_status = HAL_ERROR;
  motor_test_u4_ctrl2_read_status = HAL_ERROR;
  motor_test_u5_ctrl1_final_read_status = HAL_ERROR;

  motor_test_u4_fault_raw_frame = 0U;
  motor_test_u5_fault_raw_frame = 0U;
  motor_test_u4_ctrl1_raw_frame = 0U;
  motor_test_u5_ctrl1_raw_frame = 0U;
  motor_test_u4_ctrl1_disable_raw_frame = 0U;
  motor_test_u4_ctrl1_disabled_raw_frame = 0U;
  motor_test_u4_ctrl13_write_raw_frame = 0U;
  motor_test_u4_ctrl13_raw_frame = 0U;
  motor_test_u4_ctrl11_write_raw_frame = 0U;
  motor_test_u4_ctrl11_raw_frame = 0U;
  motor_test_u4_ctrl2_raw_frame = 0U;
  motor_test_u5_ctrl1_final_raw_frame = 0U;

  motor_test_u4_fault = 0U;
  motor_test_u5_fault = 0U;
  motor_test_u4_ctrl1_initial = 0U;
  motor_test_u5_ctrl1_initial = 0U;
  motor_test_u4_ctrl1_disabled = 0U;
  motor_test_u4_ctrl13_readback = 0U;
  motor_test_u4_ctrl11_readback = 0U;
  motor_test_u4_ctrl2_readback = 0U;
  motor_test_u5_ctrl1_final = 0U;
  motor_test_fault_x1_awake_level = GPIO_PIN_RESET;
  motor_test_fault_x1_awake_asserted = 1U;
  motor_test_failure_mask = 0U;
  motor_test_preparation_passed = 0U;
  motor_test_wake_tick_ms = 0U;
  motor_test_fault_sample_tick_ms = 0U;
}

static void Motor_Test_ClearHoldResults(void)
{
  motor_test_hold_status = HAL_ERROR;
  motor_test_hold_u5_ctrl1_read_status = HAL_ERROR;
  motor_test_hold_ctrl1_enable_write_status = HAL_ERROR;
  motor_test_hold_ctrl1_enable_read_status = HAL_ERROR;
  motor_test_hold_fault_read_status = HAL_ERROR;
  motor_test_hold_u5_ctrl1_raw_frame = 0U;
  motor_test_hold_ctrl1_enable_write_raw_frame = 0U;
  motor_test_hold_ctrl1_enable_read_raw_frame = 0U;
  motor_test_hold_fault_raw_frame = 0U;
  motor_test_hold_u5_ctrl1_before_enable = 0U;
  motor_test_hold_ctrl1_enabled_readback = 0U;
  motor_test_hold_fault = 0U;
  motor_test_hold_nfault_at_enable = GPIO_PIN_RESET;
  motor_test_hold_nfault_at_shutdown = GPIO_PIN_RESET;
  motor_test_hold_start_tick_ms = 0U;
  motor_test_hold_end_tick_ms = 0U;
  motor_test_hold_fault_sample_count = 0U;
}

static void Motor_Test_ClearSlowStepResults(void)
{
  motor_test_slow_step_status = HAL_ERROR;
  motor_test_slow_step_pulses_issued = 0U;
  motor_test_slow_step_nfault_at_shutdown = GPIO_PIN_RESET;
  motor_test_slow_step_start_tick_ms = 0U;
  motor_test_slow_step_end_tick_ms = 0U;
}

static void Motor_Test_ClearOneRevolutionResults(void)
{
  motor_test_one_rev_status = HAL_ERROR;
  motor_test_one_rev_fault_read_status = HAL_ERROR;
  motor_test_one_rev_diag2_read_status = HAL_ERROR;
  motor_test_one_rev_pulses_issued = 0U;
  motor_test_one_rev_diagnostic_sample_count = 0U;
  motor_test_one_rev_fault_raw_frame = 0U;
  motor_test_one_rev_diag2_raw_frame = 0U;
  motor_test_one_rev_fault = 0U;
  motor_test_one_rev_diag2 = 0U;
  motor_test_one_rev_otw_seen = 0U;
  motor_test_one_rev_ots_seen = 0U;
  motor_test_one_rev_nfault_at_shutdown = GPIO_PIN_RESET;
  motor_test_one_rev_start_tick_ms = 0U;
  motor_test_one_rev_end_tick_ms = 0U;

  motor_test_encoder_ready_at_arm = 0U;
  motor_test_encoder_window_started = 0U;
  motor_test_encoder_raw_before_enable = 0U;
  motor_test_encoder_raw_start = 0U;
  motor_test_encoder_raw_after_accel = 0U;
  motor_test_encoder_raw_after_cruise = 0U;
  motor_test_encoder_raw_end = 0U;
  motor_test_encoder_delta_accel = 0;
  motor_test_encoder_delta_cruise = 0;
  motor_test_encoder_delta_decel = 0;
  motor_test_encoder_delta_total = 0;
  motor_test_encoder_index_start = 0U;
  motor_test_encoder_index_end = 0U;
  motor_test_encoder_index_delta = 0U;
  motor_test_encoder_count_within_tolerance = 0U;
  motor_test_encoder_index_seen = 0U;
  motor_test_encoder_accel_end_tick_ms = 0U;
  motor_test_encoder_cruise_end_tick_ms = 0U;
}

/**
  * @brief  Provides the low portion of each safe Test 10 STEP pulse.
  */
static uint32_t Motor_Test_OneRevLowTimeMs(uint16_t pulse_index)
{
  uint32_t ramp_offset;
  uint32_t ramp_span;

  ramp_span = MOTOR_TEST_ONE_REV_LOW_START_MS - MOTOR_TEST_ONE_REV_LOW_CRUISE_MS;

  if (pulse_index < MOTOR_TEST_ONE_REV_ACCEL_STEPS)
  {
    return MOTOR_TEST_ONE_REV_LOW_START_MS -
           ((ramp_span * pulse_index) / (MOTOR_TEST_ONE_REV_ACCEL_STEPS - 1U));
  }

  if (pulse_index < (MOTOR_TEST_ONE_REV_ACCEL_STEPS + MOTOR_TEST_ONE_REV_CRUISE_STEPS))
  {
    return MOTOR_TEST_ONE_REV_LOW_CRUISE_MS;
  }

  ramp_offset = pulse_index -
                (MOTOR_TEST_ONE_REV_ACCEL_STEPS + MOTOR_TEST_ONE_REV_CRUISE_STEPS);
  return MOTOR_TEST_ONE_REV_LOW_CRUISE_MS +
         ((ramp_span * ramp_offset) / (MOTOR_TEST_ONE_REV_DECEL_STEPS - 1U));
}

/**
  * @brief  Decode captured A/B and index evidence after disabling the drivers.
  */
static void Motor_Test_DecodeEncoderEnd(bool completed)
{
  motor_test_encoder_delta_total =
      (int32_t)(motor_test_encoder_raw_end - motor_test_encoder_raw_start);
  motor_test_encoder_index_delta =
      motor_test_encoder_index_end - motor_test_encoder_index_start;
  motor_test_encoder_index_seen =
      (motor_test_encoder_index_delta != 0U) ? 1U : 0U;
  if (completed)
  {
    motor_test_encoder_delta_accel =
        (int32_t)(motor_test_encoder_raw_after_accel - motor_test_encoder_raw_start);
    motor_test_encoder_delta_cruise =
        (int32_t)(motor_test_encoder_raw_after_cruise - motor_test_encoder_raw_after_accel);
    motor_test_encoder_delta_decel =
        (int32_t)(motor_test_encoder_raw_end - motor_test_encoder_raw_after_cruise);
    motor_test_encoder_count_within_tolerance =
        Motor_Test_EncoderCountIsWithinTolerance(motor_test_encoder_delta_total);
  }
}

static uint8_t Motor_Test_EncoderCountIsWithinTolerance(int32_t count)
{
  return (((count >= (MOTOR_TEST_ENCODER_EXPECTED_COUNTS - MOTOR_TEST_ENCODER_TOLERANCE_COUNTS)) &&
           (count <= (MOTOR_TEST_ENCODER_EXPECTED_COUNTS + MOTOR_TEST_ENCODER_TOLERANCE_COUNTS))) ||
          ((count >= -(MOTOR_TEST_ENCODER_EXPECTED_COUNTS + MOTOR_TEST_ENCODER_TOLERANCE_COUNTS)) &&
           (count <= -(MOTOR_TEST_ENCODER_EXPECTED_COUNTS - MOTOR_TEST_ENCODER_TOLERANCE_COUNTS)))) ? 1U : 0U;
}

static HAL_StatusTypeDef WakeXDisabled(void)
{
  Motor_Test_ClearResults();
  Motor_Test_ForceSafeState();
  HAL_Delay(MOTOR_TEST_SLEEP_RESET_MS);

  motor_test_spi_init_status =
      ((hspi3.Instance == SPI3) && (hspi3.State == HAL_SPI_STATE_READY)) ? HAL_OK : HAL_ERROR;
  if (motor_test_spi_init_status != HAL_OK)
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_SPI_INIT;
    return HAL_ERROR;
  }

  HAL_GPIO_WritePin(SLEEP_X_GPIO_Port, SLEEP_X_Pin, GPIO_PIN_SET);
  motor_test_wake_tick_ms = HAL_GetTick();
  HAL_Delay(MOTOR_TEST_AWAKE_SETTLE_MS);
  return HAL_OK;
}

static HAL_StatusTypeDef Motor_Test_ConfigureDisabled(void)
{
  if (WakeXDisabled() != HAL_OK) return HAL_ERROR;
  motor_test_fault_sample_tick_ms = HAL_GetTick();
  motor_test_fault_x1_awake_level = HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin);
  motor_test_fault_x1_awake_asserted =
      (motor_test_fault_x1_awake_level == GPIO_PIN_RESET) ? 1U : 0U;
  if (motor_test_fault_x1_awake_asserted != 0U)
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_NFAULT;
    return HAL_ERROR;
  }

  motor_test_u4_fault_read_status =
      DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_FAULT,
                           &motor_test_u4_fault_raw_frame, &motor_test_u4_fault);
  motor_test_u5_fault_read_status =
      DRV8461_ReadRegister(CS_X2_GPIO_Port, CS_X2_Pin, DRV8461_REG_FAULT,
                           &motor_test_u5_fault_raw_frame, &motor_test_u5_fault);
  if ((motor_test_u4_fault_read_status != HAL_OK) ||
      (motor_test_u5_fault_read_status != HAL_OK) ||
      (DRV8461_FrameIsHealthy(motor_test_u4_fault_raw_frame) == 0U) ||
      (DRV8461_FrameIsHealthy(motor_test_u5_fault_raw_frame) == 0U))
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_SPI_FRAME;
    return HAL_ERROR;
  }
  if (motor_test_u4_fault != 0U)
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_U4_FAULT;
    return HAL_ERROR;
  }
  if (motor_test_u5_fault != 0U)
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_U5_FAULT;
    return HAL_ERROR;
  }

  motor_test_u4_ctrl1_read_status =
      DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL1,
                           &motor_test_u4_ctrl1_raw_frame, &motor_test_u4_ctrl1_initial);
  motor_test_u5_ctrl1_read_status =
      DRV8461_ReadRegister(CS_X2_GPIO_Port, CS_X2_Pin, DRV8461_REG_CTRL1,
                           &motor_test_u5_ctrl1_raw_frame, &motor_test_u5_ctrl1_initial);
  if ((motor_test_u4_ctrl1_read_status != HAL_OK) ||
      (motor_test_u5_ctrl1_read_status != HAL_OK) ||
      (DRV8461_FrameIsHealthy(motor_test_u4_ctrl1_raw_frame) == 0U) ||
      (DRV8461_FrameIsHealthy(motor_test_u5_ctrl1_raw_frame) == 0U))
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_SPI_FRAME;
    return HAL_ERROR;
  }
  if ((motor_test_u4_ctrl1_initial & DRV8461_CTRL1_EN_OUT_MASK) != 0U)
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_U4_EN_OUT;
    return HAL_ERROR;
  }
  if ((motor_test_u5_ctrl1_initial & DRV8461_CTRL1_EN_OUT_MASK) != 0U)
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_U5_EN_OUT;
    return HAL_ERROR;
  }

  /* Force and verify disabled H-bridges before writing current configuration. */
  motor_test_u4_ctrl1_disable_write_status =
      DRV8461_WriteRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL1,
                            DRV8461_CTRL1_DISABLED, &motor_test_u4_ctrl1_disable_raw_frame);
  motor_test_u4_ctrl1_disabled_read_status =
      DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL1,
                           &motor_test_u4_ctrl1_disabled_raw_frame,
                           &motor_test_u4_ctrl1_disabled);
  if ((motor_test_u4_ctrl1_disable_write_status != HAL_OK) ||
      (motor_test_u4_ctrl1_disabled_read_status != HAL_OK) ||
      (DRV8461_FrameIsHealthy(motor_test_u4_ctrl1_disable_raw_frame) == 0U) ||
      (DRV8461_FrameIsHealthy(motor_test_u4_ctrl1_disabled_raw_frame) == 0U) ||
      (motor_test_u4_ctrl1_disabled != DRV8461_CTRL1_DISABLED))
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_CONFIG_READBACK;
    return HAL_ERROR;
  }

  /* VREF is tied to PGND on this PCB, so select the internal 3.3-V reference. */
  motor_test_u4_ctrl13_write_status =
      DRV8461_WriteRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL13,
                            DRV8461_CTRL13_INTERNAL_VREF,
                            &motor_test_u4_ctrl13_write_raw_frame);
  motor_test_u4_ctrl13_read_status =
      DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL13,
                           &motor_test_u4_ctrl13_raw_frame,
                           &motor_test_u4_ctrl13_readback);
  if ((motor_test_u4_ctrl13_write_status != HAL_OK) ||
      (motor_test_u4_ctrl13_read_status != HAL_OK) ||
      (DRV8461_FrameIsHealthy(motor_test_u4_ctrl13_write_raw_frame) == 0U) ||
      (DRV8461_FrameIsHealthy(motor_test_u4_ctrl13_raw_frame) == 0U) ||
      (motor_test_u4_ctrl13_readback != DRV8461_CTRL13_INTERNAL_VREF))
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_CONFIG_READBACK;
    return HAL_ERROR;
  }

  /* 0x19 encodes 26/256 of the 3.3-V / 1.12-V/A full scale: about 0.30 A. */
  motor_test_u4_ctrl11_write_status =
      DRV8461_WriteRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL11,
                            DRV8461_CTRL11_030A_TRQ_DAC,
                            &motor_test_u4_ctrl11_write_raw_frame);
  motor_test_u4_ctrl11_read_status =
      DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL11,
                           &motor_test_u4_ctrl11_raw_frame,
                           &motor_test_u4_ctrl11_readback);
  if ((motor_test_u4_ctrl11_write_status != HAL_OK) ||
      (motor_test_u4_ctrl11_read_status != HAL_OK) ||
      (DRV8461_FrameIsHealthy(motor_test_u4_ctrl11_write_raw_frame) == 0U) ||
      (DRV8461_FrameIsHealthy(motor_test_u4_ctrl11_raw_frame) == 0U) ||
      (motor_test_u4_ctrl11_readback != DRV8461_CTRL11_030A_TRQ_DAC))
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_CONFIG_READBACK;
    return HAL_ERROR;
  }

  motor_test_u4_ctrl2_read_status =
      DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL2,
                           &motor_test_u4_ctrl2_raw_frame, &motor_test_u4_ctrl2_readback);
  motor_test_u5_ctrl1_final_read_status =
      DRV8461_ReadRegister(CS_X2_GPIO_Port, CS_X2_Pin, DRV8461_REG_CTRL1,
                           &motor_test_u5_ctrl1_final_raw_frame,
                           &motor_test_u5_ctrl1_final);
  if ((motor_test_u4_ctrl2_read_status != HAL_OK) ||
      (motor_test_u5_ctrl1_final_read_status != HAL_OK) ||
      (DRV8461_FrameIsHealthy(motor_test_u4_ctrl2_raw_frame) == 0U) ||
      (DRV8461_FrameIsHealthy(motor_test_u5_ctrl1_final_raw_frame) == 0U) ||
      (motor_test_u4_ctrl2_readback != DRV8461_CTRL2_RESET_VALUE) ||
      ((motor_test_u5_ctrl1_final & DRV8461_CTRL1_EN_OUT_MASK) != 0U))
  {
    motor_test_failure_mask |= MOTOR_TEST_FAIL_CONFIG_READBACK;
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef DRV8461_ReadRegister(GPIO_TypeDef *cs_port,
                                               uint16_t cs_pin,
                                               uint8_t address,
                                               volatile uint16_t *raw_frame,
                                               volatile uint8_t *register_value)
{
  uint8_t tx_data[2] = {(uint8_t)(0x40U | (address & 0x3FU)), 0x00U};
  uint8_t rx_data[2] = {0U, 0U};
  HAL_StatusTypeDef transfer_status;

  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  transfer_status = HAL_SPI_TransmitReceive(&hspi3, tx_data, rx_data, 2U, DRV8461_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  *raw_frame = (uint16_t)(((uint16_t)rx_data[0] << 8U) | rx_data[1]);
  *register_value = rx_data[1];
  HAL_Delay(1U);
  return transfer_status;
}

static HAL_StatusTypeDef DRV8461_WriteRegister(GPIO_TypeDef *cs_port,
                                                uint16_t cs_pin,
                                                uint8_t address,
                                                uint8_t value,
                                                volatile uint16_t *raw_frame)
{
  uint8_t tx_data[2] = {(uint8_t)(address & 0x3FU), value};
  uint8_t rx_data[2] = {0U, 0U};
  HAL_StatusTypeDef transfer_status;

  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  transfer_status = HAL_SPI_TransmitReceive(&hspi3, tx_data, rx_data, 2U, DRV8461_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  *raw_frame = (uint16_t)(((uint16_t)rx_data[0] << 8U) | rx_data[1]);
  HAL_Delay(1U);
  return transfer_status;
}

static bool DRV8461_FrameIsValid(uint16_t raw_frame)
{
  uint8_t status_byte = (uint8_t)(raw_frame >> 8U);
  return raw_frame != 0x0000U && raw_frame != 0xFFFFU &&
         (status_byte & DRV8461_STATUS_HEADER_MASK) == DRV8461_STATUS_HEADER_VALUE;
}

static uint8_t DRV8461_FrameIsHealthy(uint16_t raw_frame)
{
  return DRV8461_FrameIsValid(raw_frame) &&
         (((raw_frame >> 8U) & DRV8461_STATUS_FAULT_MASK) == 0U);
}

/**
 * @brief  Recheck the shared X axis and enable only U4 for an accepted request.
 */
static MotorMoveResult Motor_Test_EnableU4Outputs(bool (*stop_requested)(void))
{
  /* Recheck U5 immediately before raising the shared external X enable. */
  motor_test_hold_u5_ctrl1_read_status =
      DRV8461_ReadRegister(CS_X2_GPIO_Port, CS_X2_Pin, DRV8461_REG_CTRL1,
                           &motor_test_hold_u5_ctrl1_raw_frame,
                           &motor_test_hold_u5_ctrl1_before_enable);
  if ((motor_test_hold_u5_ctrl1_read_status != HAL_OK) ||
      (DRV8461_FrameIsHealthy(motor_test_hold_u5_ctrl1_raw_frame) == 0U) ||
      ((motor_test_hold_u5_ctrl1_before_enable & DRV8461_CTRL1_EN_OUT_MASK) != 0U))
  {
    return MOTOR_MOVE_ENABLE_FAILED;
  }

  /* External MOTION_OK must be a logic HIGH for actual torque; the MCU cannot read it. */
  MotorMoveResult result = MotionGuard(stop_requested);
  if (result != MOTOR_MOVE_DONE)
    return result;
  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_SET);
  HAL_Delay(1U);
  result = MotionGuard(stop_requested);
  motor_test_hold_nfault_at_enable = motor_test_one_rev_nfault_at_shutdown;
  if (result != MOTOR_MOVE_DONE)
    return result;

  /* The active application consumes its one-shot authorization before this write. */
  motor_test_hold_ctrl1_enable_write_status =
      DRV8461_WriteRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL1,
                            DRV8461_CTRL1_ENABLE_OUTPUTS,
                            &motor_test_hold_ctrl1_enable_write_raw_frame);
  if (motor_test_hold_ctrl1_enable_write_status != HAL_OK ||
      !DRV8461_FrameIsHealthy(motor_test_hold_ctrl1_enable_write_raw_frame))
    return MOTOR_MOVE_ENABLE_FAILED;
  motor_test_hold_ctrl1_enable_read_status =
      DRV8461_ReadRegister(CS_X1_GPIO_Port, CS_X1_Pin, DRV8461_REG_CTRL1,
                           &motor_test_hold_ctrl1_enable_read_raw_frame,
                           &motor_test_hold_ctrl1_enabled_readback);
  if ((motor_test_hold_ctrl1_enable_read_status != HAL_OK) ||
      (DRV8461_FrameIsHealthy(motor_test_hold_ctrl1_enable_read_raw_frame) == 0U) ||
      (motor_test_hold_ctrl1_enabled_readback != DRV8461_CTRL1_ENABLE_OUTPUTS))
  {
    return MOTOR_MOVE_ENABLE_FAILED;
  }

  return MOTOR_MOVE_DONE;
}

static void Motor_Test_ForceSafeState(void)
{
  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Y_MCU_GPIO_Port, ENA_Y_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Z_MCU_GPIO_Port, ENA_Z_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DIR_X_GPIO_Port, DIR_X_Pin | STEP_X_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(CS_X1_GPIO_Port, CS_X1_Pin | CS_X2_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Y_GPIO_Port, CS_Y_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Z_GPIO_Port, CS_Z_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(SLEEP_X_GPIO_Port, SLEEP_X_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SLEEP_Y_GPIO_Port, SLEEP_Y_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SLEEP_Z_GPIO_Port, SLEEP_Z_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
}
