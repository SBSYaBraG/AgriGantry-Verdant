/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : interlock_diag.h
  * @brief          : Safe X-axis MOTION_OK interlock observation interface.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef INTERLOCK_DIAG_H
#define INTERLOCK_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Retained state for supervised SRAM inspection. */
extern volatile uint8_t interlock_test_initialized;
extern volatile GPIO_PinState interlock_fault_x1_awake_level;
extern volatile uint8_t interlock_fault_x1_awake_asserted;
extern volatile uint8_t interlock_ena_x_mcu_command_high;
extern volatile uint32_t interlock_wake_tick_ms;
extern volatile uint32_t interlock_fault_x1_awake_tick_ms;
extern volatile uint32_t interlock_last_ena_x_transition_tick_ms;
extern volatile uint32_t interlock_ena_x_high_cycle_count;

void Interlock_Diagnostic_Initialize(void);
void Interlock_Diagnostic_PulseXEnable(void);
void Interlock_Diagnostic_IndicateAwakeFault(void);

#ifdef __cplusplus
}
#endif

#endif /* INTERLOCK_DIAG_H */
