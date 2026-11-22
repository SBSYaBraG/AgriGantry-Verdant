/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : spi_diag.h
  * @brief          : Read-only DRV8461 U4 SPI diagnostic interface.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef SPI_DIAG_H
#define SPI_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Raw U4 responses and decoded fields retained for future SWD inspection. */
extern volatile HAL_StatusTypeDef drv8461_spi_init_status;
extern volatile HAL_StatusTypeDef drv8461_fault_read_status;
extern volatile HAL_StatusTypeDef drv8461_diag1_read_status;
extern volatile HAL_StatusTypeDef drv8461_diag2_read_status;
extern volatile HAL_StatusTypeDef drv8461_diag3_read_status;
extern volatile HAL_StatusTypeDef drv8461_fault_recheck_read_status;
extern volatile HAL_StatusTypeDef drv8461_ctrl1_read_status;
extern volatile HAL_StatusTypeDef drv8461_ctrl2_read_status;

extern volatile uint16_t drv8461_fault_raw_frame;
extern volatile uint16_t drv8461_diag1_raw_frame;
extern volatile uint16_t drv8461_diag2_raw_frame;
extern volatile uint16_t drv8461_diag3_raw_frame;
extern volatile uint16_t drv8461_fault_recheck_raw_frame;
extern volatile uint16_t drv8461_ctrl1_raw_frame;
extern volatile uint16_t drv8461_ctrl2_raw_frame;

extern volatile uint8_t drv8461_fault_status_byte;
extern volatile uint8_t drv8461_diag1_status_byte;
extern volatile uint8_t drv8461_diag2_status_byte;
extern volatile uint8_t drv8461_diag3_status_byte;
extern volatile uint8_t drv8461_fault_recheck_status_byte;
extern volatile uint8_t drv8461_ctrl1_status_byte;
extern volatile uint8_t drv8461_ctrl2_status_byte;
extern volatile uint8_t drv8461_fault_register;
extern volatile uint8_t drv8461_diag1_register;
extern volatile uint8_t drv8461_diag2_register;
extern volatile uint8_t drv8461_diag3_register;
extern volatile uint8_t drv8461_fault_recheck_register;
extern volatile uint8_t drv8461_ctrl1_register;
extern volatile uint8_t drv8461_ctrl2_register;
extern volatile uint8_t drv8461_status_fault_bits;

extern volatile GPIO_PinState drv8461_fault_x1_pre_read_level;
extern volatile uint8_t drv8461_fault_x1_pre_read_asserted;
extern volatile GPIO_PinState drv8461_fault_x1_post_read_level;
extern volatile uint8_t drv8461_fault_x1_post_read_asserted;
extern volatile GPIO_PinState drv8461_fault_x1_level;
extern volatile uint8_t drv8461_fault_x1_asserted;
extern volatile uint8_t drv8461_status_headers_valid;
extern volatile uint8_t drv8461_raw_response_invalid;
extern volatile uint8_t drv8461_register_defaults_match;
extern volatile uint8_t drv8461_spi_communication_valid;
extern volatile uint8_t drv8461_diagnostic_fault_indicated;
extern volatile uint8_t drv8461_fault_summary_only;
extern volatile uint8_t drv8461_spi_test_passed;

extern volatile uint32_t drv8461_wake_tick_ms;
extern volatile uint32_t drv8461_fault_x1_pre_read_tick_ms;
extern volatile uint32_t drv8461_fault_initial_tick_ms;
extern volatile uint32_t drv8461_diag1_tick_ms;
extern volatile uint32_t drv8461_diag2_tick_ms;
extern volatile uint32_t drv8461_diag3_tick_ms;
extern volatile uint32_t drv8461_fault_recheck_delay_start_tick_ms;
extern volatile uint32_t drv8461_fault_recheck_tick_ms;
extern volatile uint32_t drv8461_fault_x1_post_read_tick_ms;
extern volatile uint32_t drv8461_ctrl1_tick_ms;
extern volatile uint32_t drv8461_ctrl2_tick_ms;
extern volatile uint32_t drv8461_fault_recheck_elapsed_ms;

void SPI_Diagnostic_Run(void);
void SPI_Diagnostic_IndicatePass(void);
void SPI_Diagnostic_IndicateDiagnosticIssue(void);
void SPI_Diagnostic_IndicateFail(void);

#ifdef __cplusplus
}
#endif

#endif /* SPI_DIAG_H */
