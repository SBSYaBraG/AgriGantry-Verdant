/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : multi_driver_diag.h
  * @brief          : Read-only all-DRV8461 SPI communication diagnostic.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef MULTI_DRIVER_DIAG_H
#define MULTI_DRIVER_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Driver result array order: 0=U4/X1, 1=U5/X2, 2=U6/Y, 3=U7/Z. */
#define MULTI_DRIVER_DIAG_DRIVER_COUNT          4U

/* multi_driver_diag_summary.overall_result values. */
#define MULTI_DRIVER_DIAG_RESULT_NOT_RUN         0U
#define MULTI_DRIVER_DIAG_RESULT_PASS            1U
#define MULTI_DRIVER_DIAG_RESULT_DIAGNOSTIC      2U
#define MULTI_DRIVER_DIAG_RESULT_COMM_FAILURE    3U

/* HAL status fields use 0=HAL_OK, 1=HAL_ERROR, 2=HAL_BUSY, 3=HAL_TIMEOUT.
 * A value of 0xFF means that the corresponding transaction did not run. */
#define MULTI_DRIVER_DIAG_STATUS_NOT_RUN         0xFFU

/**
 * @brief One retained result record for a single DRV8461.
 *
 * Raw frames use the lower 16 bits: upper byte is the returned status and
 * lower byte is the requested register value.  All multi-byte fields are
 * naturally 32-bit aligned to make Hot Plug SRAM inspection unambiguous.
 */
typedef struct
{
  volatile uint32_t fault_first_raw_frame;
  volatile uint32_t ctrl1_raw_frame;
  volatile uint32_t ctrl2_raw_frame;
  volatile uint32_t fault_recheck_raw_frame;
  volatile uint32_t fault_first_tick_ms;
  volatile uint32_t ctrl1_tick_ms;
  volatile uint32_t ctrl2_tick_ms;
  volatile uint32_t fault_recheck_tick_ms;
  volatile uint8_t fault_first_hal_status;
  volatile uint8_t ctrl1_hal_status;
  volatile uint8_t ctrl2_hal_status;
  volatile uint8_t fault_recheck_hal_status;
  volatile uint8_t early_nfault_level;
  volatile uint8_t settled_nfault_level;
  volatile uint8_t final_nfault_level;
  volatile uint8_t valid_status_header_mask;
  volatile uint8_t fault_first_value;
  volatile uint8_t ctrl1_value;
  volatile uint8_t ctrl2_value;
  volatile uint8_t fault_recheck_value;
  volatile uint8_t status_fault_bits_or;
  volatile uint8_t raw_frame_invalid;
  volatile uint8_t communication_valid;
  volatile uint8_t control_defaults_valid;
  volatile uint8_t diagnostic_indicated;
} MultiDriverDiagDriverResult;

/**
 * @brief Retained overall Test 11 state.  Per-driver masks use bit 0=U4,
 *        bit 1=U5, bit 2=U6, and bit 3=U7.
 */
typedef struct
{
  volatile uint32_t signature;
  volatile uint32_t started;
  volatile uint32_t completed;
  volatile uint32_t overall_result;
  volatile uint32_t spi_init_status;
  volatile uint32_t wake_tick_ms;
  volatile uint32_t early_fault_sample_tick_ms;
  volatile uint32_t settled_fault_sample_tick_ms;
  volatile uint32_t fault_recheck_delay_tick_ms;
  volatile uint32_t shutdown_tick_ms;
  volatile uint32_t communication_failure_mask;
  volatile uint32_t diagnostic_mask;
  volatile uint32_t control_default_mismatch_mask;
  volatile uint32_t early_nfault_asserted_mask;
  volatile uint32_t settled_nfault_asserted_mask;
  volatile uint32_t final_nfault_asserted_mask;
  volatile uint32_t safe_shutdown_confirmed;
} MultiDriverDiagSummary;

extern volatile MultiDriverDiagSummary multi_driver_diag_summary;
extern volatile MultiDriverDiagDriverResult
    multi_driver_diag_driver[MULTI_DRIVER_DIAG_DRIVER_COUNT];

void Multi_Driver_Diagnostic_Run(void);
void Multi_Driver_Diagnostic_Service(void);

#ifdef __cplusplus
}
#endif

#endif /* MULTI_DRIVER_DIAG_H */
