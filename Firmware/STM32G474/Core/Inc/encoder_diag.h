/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : encoder_diag.h
  * @brief          : Encoder observation diagnostic interface.
  ******************************************************************************
  * @attention
  *
  * This diagnostic observes U13/TIM5 encoder signals only.  It never enables
  * a motor driver, changes MOTION_OK, performs an SPI transaction, or emits a
  * STEP pulse.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef ENCODER_DIAG_H
#define ENCODER_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*
 * Retained live capture values.  Read these through SWD while the firmware is
 * running (or after a Hot Plug halt); they are normal SRAM variables and are
 * cleared by reset.
 */
extern volatile uint32_t encoder_diag_signature;
extern volatile uint8_t encoder_diag_initialized;
extern volatile HAL_StatusTypeDef encoder_diag_timer_start_status;
extern volatile uint32_t encoder_diag_start_tick_ms;
extern volatile uint32_t encoder_diag_last_sample_tick_ms;
extern volatile uint32_t encoder_diag_raw_count;
extern volatile int32_t encoder_diag_signed_count;
extern volatile int32_t encoder_diag_last_delta;
extern volatile int8_t encoder_diag_direction;
extern volatile int8_t encoder_diag_last_motion_direction;
extern volatile uint32_t encoder_diag_sample_count;
extern volatile uint32_t encoder_diag_index_event_count;
extern volatile uint32_t encoder_diag_last_index_tick_ms;
extern volatile uint32_t encoder_diag_raw_count_at_last_index;
extern volatile uint8_t encoder_diag_index_pin_level;
extern volatile uint8_t encoder_diag_led_state;

void Encoder_Diagnostic_Init(void);
void Encoder_Diagnostic_Service(void);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_DIAG_H */
