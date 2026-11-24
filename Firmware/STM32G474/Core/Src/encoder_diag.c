/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : encoder_diag.c
  * @brief          : Encoder observation application code.
  ******************************************************************************
  * @attention
  *
  * TIM5 is configured by CubeMX in TI1-and-TI2 encoder mode:
  *   PA0 / TIM5_CH1 = MX1_ENC_B
  *   PA1 / TIM5_CH2 = MX1_ENC_A
  * PA2 / MX1_ENC_Z is a rising-edge EXTI input.
  *
  * This file is application-owned rather than CubeMX-generated.  It contains
  * no DRV8461 SPI access and never asserts a motor enable or STEP signal.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "encoder_diag.h"
#include "tim.h"

#define ENCODER_DIAG_SIGNATURE_VALUE       0x454E4339UL /* "ENC9" */
#define ENCODER_DIAG_HEARTBEAT_PERIOD_MS   2000UL
#define ENCODER_DIAG_HEARTBEAT_ON_MS        200UL

/* Direction encoding retained in encoder_diag_direction fields. */
#define ENCODER_DIAG_DIRECTION_STOPPED        0
#define ENCODER_DIAG_DIRECTION_POSITIVE       1
#define ENCODER_DIAG_DIRECTION_NEGATIVE      -1

volatile uint32_t encoder_diag_signature;
volatile uint8_t encoder_diag_initialized;
volatile HAL_StatusTypeDef encoder_diag_timer_start_status;
volatile uint32_t encoder_diag_start_tick_ms;
volatile uint32_t encoder_diag_last_sample_tick_ms;
volatile uint32_t encoder_diag_raw_count;
volatile int32_t encoder_diag_signed_count;
volatile int32_t encoder_diag_last_delta;
volatile int8_t encoder_diag_direction;
volatile int8_t encoder_diag_last_motion_direction;
volatile uint32_t encoder_diag_sample_count;
volatile uint32_t encoder_diag_index_event_count;
volatile uint32_t encoder_diag_last_index_tick_ms;
volatile uint32_t encoder_diag_raw_count_at_last_index;
volatile uint8_t encoder_diag_index_pin_level;
volatile uint8_t encoder_diag_led_state;

static uint32_t encoder_diag_previous_raw_count;
static uint32_t encoder_diag_last_heartbeat_tick_ms;

/**
  * @brief  Reassert the board's disabled motor state before observing encoder
  *         inputs.  This does not access, read, or alter MOTION_OK.
  */
static void Encoder_Diagnostic_ForceSafeMotorState(void)
{
  HAL_GPIO_WritePin(DIR_X_GPIO_Port, DIR_X_Pin | STEP_X_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Y_MCU_GPIO_Port, ENA_Y_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Z_MCU_GPIO_Port, ENA_Z_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SLEEP_X_GPIO_Port, SLEEP_X_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SLEEP_Y_GPIO_Port, SLEEP_Y_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SLEEP_Z_GPIO_Port, SLEEP_Z_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(CS_X1_GPIO_Port, CS_X1_Pin | CS_X2_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Y_GPIO_Port, CS_Y_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Z_GPIO_Port, CS_Z_Pin, GPIO_PIN_SET);
}

/**
  * @brief  Updates a nonblocking D5 heartbeat.  A 200-ms flash every two
  *         seconds means TIM5 started; it is not an encoder-pass assertion.
  */
static void Encoder_Diagnostic_UpdateLed(uint32_t now_ms)
{
  uint8_t led_on;

  if (encoder_diag_timer_start_status != HAL_OK)
  {
    /* Timer-start failure: distinguishable rapid continuous blinking. */
    led_on = ((now_ms / 100UL) & 1UL) != 0UL;
  }
  else
  {
    if ((now_ms - encoder_diag_last_heartbeat_tick_ms) >= ENCODER_DIAG_HEARTBEAT_PERIOD_MS)
    {
      encoder_diag_last_heartbeat_tick_ms = now_ms;
    }

    led_on = ((now_ms - encoder_diag_last_heartbeat_tick_ms) < ENCODER_DIAG_HEARTBEAT_ON_MS);
  }

  encoder_diag_led_state = led_on;
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin,
                    (led_on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Encoder_Diagnostic_StartCounter(void)
{
  encoder_diag_initialized = 0U;
  encoder_diag_index_event_count = 0U;
  __HAL_TIM_SET_COUNTER(&htim5, 0U);
  encoder_diag_timer_start_status = HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);
  encoder_diag_initialized = (encoder_diag_timer_start_status == HAL_OK);
}

void Encoder_Diagnostic_Init(void)
{
  uint32_t now_ms;

  Encoder_Diagnostic_ForceSafeMotorState();

  encoder_diag_signature = ENCODER_DIAG_SIGNATURE_VALUE;
  encoder_diag_start_tick_ms = 0U;
  encoder_diag_last_sample_tick_ms = 0U;
  encoder_diag_raw_count = 0U;
  encoder_diag_signed_count = 0;
  encoder_diag_last_delta = 0;
  encoder_diag_direction = ENCODER_DIAG_DIRECTION_STOPPED;
  encoder_diag_last_motion_direction = ENCODER_DIAG_DIRECTION_STOPPED;
  encoder_diag_sample_count = 0U;
  encoder_diag_last_index_tick_ms = 0U;
  encoder_diag_raw_count_at_last_index = 0U;
  encoder_diag_index_pin_level = (uint8_t)HAL_GPIO_ReadPin(MX1_ENC_Z_GPIO_Port, MX1_ENC_Z_Pin);
  encoder_diag_led_state = 0U;

  /* Establish a count origin without touching any motor-control peripheral. */
  Encoder_Diagnostic_StartCounter();

  now_ms = HAL_GetTick();
  encoder_diag_start_tick_ms = now_ms;
  encoder_diag_last_sample_tick_ms = now_ms;
  encoder_diag_last_heartbeat_tick_ms = now_ms;
  encoder_diag_raw_count = __HAL_TIM_GET_COUNTER(&htim5);
  encoder_diag_previous_raw_count = encoder_diag_raw_count;

  Encoder_Diagnostic_UpdateLed(now_ms);
}

/**
  * @brief  Samples TIM5, converts modular 32-bit timer movement into a signed
  *         count change, and publishes it in the retained diagnostic block.
  */
void Encoder_Diagnostic_Service(void)
{
  uint32_t now_ms;
  uint32_t raw_count;
  int32_t count_delta;

  now_ms = HAL_GetTick();

  if (encoder_diag_timer_start_status == HAL_OK)
  {
    raw_count = __HAL_TIM_GET_COUNTER(&htim5);
    count_delta = (int32_t)(raw_count - encoder_diag_previous_raw_count);

    encoder_diag_raw_count = raw_count;
    encoder_diag_last_delta = count_delta;

    if (count_delta > 0)
    {
      encoder_diag_signed_count += count_delta;
      encoder_diag_direction = ENCODER_DIAG_DIRECTION_POSITIVE;
      encoder_diag_last_motion_direction = ENCODER_DIAG_DIRECTION_POSITIVE;
    }
    else if (count_delta < 0)
    {
      encoder_diag_signed_count += count_delta;
      encoder_diag_direction = ENCODER_DIAG_DIRECTION_NEGATIVE;
      encoder_diag_last_motion_direction = ENCODER_DIAG_DIRECTION_NEGATIVE;
    }
    else
    {
      encoder_diag_direction = ENCODER_DIAG_DIRECTION_STOPPED;
    }

    encoder_diag_previous_raw_count = raw_count;
    encoder_diag_last_sample_tick_ms = now_ms;
    encoder_diag_sample_count++;
  }

  Encoder_Diagnostic_UpdateLed(now_ms);
}

/**
  * @brief  Cube HAL EXTI callback for the optional encoder index input.
  * @note   Index events are recorded but never reset or alter the A/B count.
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == MX1_ENC_Z_Pin && encoder_diag_initialized != 0U)
  {
    encoder_diag_index_event_count++;
    encoder_diag_last_index_tick_ms = HAL_GetTick();
    encoder_diag_raw_count_at_last_index = __HAL_TIM_GET_COUNTER(&htim5);
    encoder_diag_index_pin_level = (uint8_t)HAL_GPIO_ReadPin(MX1_ENC_Z_GPIO_Port, MX1_ENC_Z_Pin);
  }
}
