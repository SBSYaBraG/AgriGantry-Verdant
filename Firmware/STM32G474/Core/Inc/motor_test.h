/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : motor_test.h
 * @brief          : Default-safe U4 preparation and explicitly armed Test 10
 *                   trajectory-plus-encoder interface.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef MOTOR_TEST_H
#define MOTOR_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Preparation telemetry retained in SRAM for supervised inspection. */
extern volatile HAL_StatusTypeDef motor_test_spi_init_status;
extern volatile HAL_StatusTypeDef motor_test_u4_fault_read_status;
extern volatile HAL_StatusTypeDef motor_test_u5_fault_read_status;
extern volatile HAL_StatusTypeDef motor_test_u4_ctrl1_read_status;
extern volatile HAL_StatusTypeDef motor_test_u5_ctrl1_read_status;
extern volatile HAL_StatusTypeDef motor_test_u4_ctrl1_disable_write_status;
extern volatile HAL_StatusTypeDef motor_test_u4_ctrl1_disabled_read_status;
extern volatile HAL_StatusTypeDef motor_test_u4_ctrl13_write_status;
extern volatile HAL_StatusTypeDef motor_test_u4_ctrl13_read_status;
extern volatile HAL_StatusTypeDef motor_test_u4_ctrl11_write_status;
extern volatile HAL_StatusTypeDef motor_test_u4_ctrl11_read_status;
extern volatile HAL_StatusTypeDef motor_test_u4_ctrl2_read_status;
extern volatile HAL_StatusTypeDef motor_test_u5_ctrl1_final_read_status;

extern volatile uint16_t motor_test_u4_fault_raw_frame;
extern volatile uint16_t motor_test_u5_fault_raw_frame;
extern volatile uint16_t motor_test_u4_ctrl1_raw_frame;
extern volatile uint16_t motor_test_u5_ctrl1_raw_frame;
extern volatile uint16_t motor_test_u4_ctrl1_disable_raw_frame;
extern volatile uint16_t motor_test_u4_ctrl1_disabled_raw_frame;
extern volatile uint16_t motor_test_u4_ctrl13_write_raw_frame;
extern volatile uint16_t motor_test_u4_ctrl13_raw_frame;
extern volatile uint16_t motor_test_u4_ctrl11_write_raw_frame;
extern volatile uint16_t motor_test_u4_ctrl11_raw_frame;
extern volatile uint16_t motor_test_u4_ctrl2_raw_frame;
extern volatile uint16_t motor_test_u5_ctrl1_final_raw_frame;

extern volatile uint8_t motor_test_u4_fault;
extern volatile uint8_t motor_test_u5_fault;
extern volatile uint8_t motor_test_u4_ctrl1_initial;
extern volatile uint8_t motor_test_u5_ctrl1_initial;
extern volatile uint8_t motor_test_u4_ctrl1_disabled;
extern volatile uint8_t motor_test_u4_ctrl13_readback;
extern volatile uint8_t motor_test_u4_ctrl11_readback;
extern volatile uint8_t motor_test_u4_ctrl2_readback;
extern volatile uint8_t motor_test_u5_ctrl1_final;
extern volatile GPIO_PinState motor_test_fault_x1_awake_level;
extern volatile uint8_t motor_test_fault_x1_awake_asserted;
extern volatile uint32_t motor_test_failure_mask;
extern volatile uint8_t motor_test_preparation_passed;
extern volatile uint32_t motor_test_wake_tick_ms;
extern volatile uint32_t motor_test_fault_sample_tick_ms;

/*
 * Test 8 arm interface.  Both values are zeroed during normal reset
 * initialization.  An operator must deliberately write the pair through SWD
 * after boot before a single bounded one-revolution test can start.
 */
#define MOTOR_TEST_HOLD_ARM_MAGIC             0x4D544831UL /* "MTH1" */
#define MOTOR_TEST_HOLD_ARM_MAGIC_COMPLEMENT  0xB2ABB7CEUL

#define MOTOR_TEST_HOLD_WAITING               0U
#define MOTOR_TEST_HOLD_ACTIVE                1U
#define MOTOR_TEST_HOLD_PASS                  2U
#define MOTOR_TEST_HOLD_FAIL                  3U
#define MOTOR_TEST_HOLD_PREPARATION_FAIL      4U

extern volatile uint32_t motor_test_hold_arm_key;
extern volatile uint32_t motor_test_hold_arm_key_complement;
extern volatile uint8_t motor_test_hold_state;
extern volatile HAL_StatusTypeDef motor_test_hold_status;
extern volatile HAL_StatusTypeDef motor_test_hold_u5_ctrl1_read_status;
extern volatile HAL_StatusTypeDef motor_test_hold_ctrl1_enable_write_status;
extern volatile HAL_StatusTypeDef motor_test_hold_ctrl1_enable_read_status;
extern volatile HAL_StatusTypeDef motor_test_hold_fault_read_status;
extern volatile uint16_t motor_test_hold_u5_ctrl1_raw_frame;
extern volatile uint16_t motor_test_hold_ctrl1_enable_write_raw_frame;
extern volatile uint16_t motor_test_hold_ctrl1_enable_read_raw_frame;
extern volatile uint16_t motor_test_hold_fault_raw_frame;
extern volatile uint8_t motor_test_hold_u5_ctrl1_before_enable;
extern volatile uint8_t motor_test_hold_ctrl1_enabled_readback;
extern volatile uint8_t motor_test_hold_fault;
extern volatile GPIO_PinState motor_test_hold_nfault_at_enable;
extern volatile GPIO_PinState motor_test_hold_nfault_at_shutdown;
extern volatile uint32_t motor_test_hold_start_tick_ms;
extern volatile uint32_t motor_test_hold_end_tick_ms;
extern volatile uint32_t motor_test_hold_fault_sample_count;

/* Test 7 motion telemetry retained for a read-only post-test SRAM capture. */
extern volatile HAL_StatusTypeDef motor_test_slow_step_status;
extern volatile uint8_t motor_test_slow_step_pulses_issued;
extern volatile GPIO_PinState motor_test_slow_step_nfault_at_shutdown;
extern volatile uint32_t motor_test_slow_step_start_tick_ms;
extern volatile uint32_t motor_test_slow_step_end_tick_ms;

/* Test 8 motion and thermal-diagnostic telemetry for a post-run SRAM read. */
extern volatile HAL_StatusTypeDef motor_test_one_rev_status;
extern volatile HAL_StatusTypeDef motor_test_one_rev_fault_read_status;
extern volatile HAL_StatusTypeDef motor_test_one_rev_diag2_read_status;
extern volatile uint16_t motor_test_one_rev_pulses_issued;
extern volatile uint16_t motor_test_one_rev_diagnostic_sample_count;
extern volatile uint16_t motor_test_one_rev_fault_raw_frame;
extern volatile uint16_t motor_test_one_rev_diag2_raw_frame;
extern volatile uint8_t motor_test_one_rev_fault;
extern volatile uint8_t motor_test_one_rev_diag2;
extern volatile uint8_t motor_test_one_rev_otw_seen;
extern volatile uint8_t motor_test_one_rev_ots_seen;
extern volatile GPIO_PinState motor_test_one_rev_nfault_at_shutdown;
extern volatile uint32_t motor_test_one_rev_start_tick_ms;
extern volatile uint32_t motor_test_one_rev_end_tick_ms;

/* Test 10 encoder snapshots for the bounded acceleration/cruise/deceleration move. */
extern volatile uint8_t motor_test_encoder_ready_at_arm;
extern volatile uint32_t motor_test_encoder_raw_before_enable;
extern volatile uint32_t motor_test_encoder_raw_start;
extern volatile uint32_t motor_test_encoder_raw_after_accel;
extern volatile uint32_t motor_test_encoder_raw_after_cruise;
extern volatile uint32_t motor_test_encoder_raw_end;
extern volatile int32_t motor_test_encoder_delta_accel;
extern volatile int32_t motor_test_encoder_delta_cruise;
extern volatile int32_t motor_test_encoder_delta_decel;
extern volatile int32_t motor_test_encoder_delta_total;
extern volatile uint32_t motor_test_encoder_index_start;
extern volatile uint32_t motor_test_encoder_index_end;
extern volatile uint32_t motor_test_encoder_index_delta;
extern volatile uint8_t motor_test_encoder_count_within_tolerance;
extern volatile uint8_t motor_test_encoder_index_seen;
extern volatile uint32_t motor_test_encoder_accel_end_tick_ms;
extern volatile uint32_t motor_test_encoder_cruise_end_tick_ms;

void Motor_Test_Prepare(void);
void Motor_Test_Service(void);
void Motor_Test_IndicateWaiting(void);
void Motor_Test_IndicatePass(void);
void Motor_Test_IndicateFailure(void);

/* Earlier bounded-test routines remain preserved; Test 8 selects one-revolution only. */
HAL_StatusTypeDef Motor_Test_RunHoldingTorque(void);
HAL_StatusTypeDef Motor_Test_RunSlowStep(void);
HAL_StatusTypeDef Motor_Test_RunOneRevolution(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_TEST_H */
