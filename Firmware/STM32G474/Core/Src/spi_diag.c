/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : spi_diag.c
  * @brief          : One-shot, read-only SPI3 diagnostic for DRV8461 U4.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "spi_diag.h"
#include "spi.h"

#define DRV8461_REG_FAULT               0x00U
#define DRV8461_REG_DIAG1               0x01U
#define DRV8461_REG_DIAG2               0x02U
#define DRV8461_REG_DIAG3               0x03U
#define DRV8461_REG_CTRL1               0x04U
#define DRV8461_REG_CTRL2               0x05U
#define DRV8461_CTRL1_RESET_VALUE       0x0FU
#define DRV8461_CTRL2_RESET_VALUE       0x06U
#define DRV8461_READ_COMMAND_BIT        0x40U
#define DRV8461_STATUS_HEADER_MASK      0xC0U
#define DRV8461_STATUS_HEADER_VALUE     0xC0U
#define DRV8461_STATUS_FAULT_MASK       0x3FU
#define DRV8461_SPI_TIMEOUT_MS          10U
#define DRV8461_WAKE_DELAY_MS           2U
#define DRV8461_FRAME_GAP_MS            1U
#define DRV8461_FAULT_RECHECK_DELAY_MS  10U
#define DRV8461_LED_PASS_FLASH_MS        80U
#define DRV8461_LED_ISSUE_ON_MS          300U
#define DRV8461_LED_ISSUE_OFF_MS         200U
#define DRV8461_LED_FAIL_ON_MS           1000U
#define DRV8461_LED_PATTERN_PAUSE_MS     2000U

volatile HAL_StatusTypeDef drv8461_spi_init_status;
volatile HAL_StatusTypeDef drv8461_fault_read_status;
volatile HAL_StatusTypeDef drv8461_diag1_read_status;
volatile HAL_StatusTypeDef drv8461_diag2_read_status;
volatile HAL_StatusTypeDef drv8461_diag3_read_status;
volatile HAL_StatusTypeDef drv8461_fault_recheck_read_status;
volatile HAL_StatusTypeDef drv8461_ctrl1_read_status;
volatile HAL_StatusTypeDef drv8461_ctrl2_read_status;

volatile uint16_t drv8461_fault_raw_frame;
volatile uint16_t drv8461_diag1_raw_frame;
volatile uint16_t drv8461_diag2_raw_frame;
volatile uint16_t drv8461_diag3_raw_frame;
volatile uint16_t drv8461_fault_recheck_raw_frame;
volatile uint16_t drv8461_ctrl1_raw_frame;
volatile uint16_t drv8461_ctrl2_raw_frame;

volatile uint8_t drv8461_fault_status_byte;
volatile uint8_t drv8461_diag1_status_byte;
volatile uint8_t drv8461_diag2_status_byte;
volatile uint8_t drv8461_diag3_status_byte;
volatile uint8_t drv8461_fault_recheck_status_byte;
volatile uint8_t drv8461_ctrl1_status_byte;
volatile uint8_t drv8461_ctrl2_status_byte;
volatile uint8_t drv8461_fault_register;
volatile uint8_t drv8461_diag1_register;
volatile uint8_t drv8461_diag2_register;
volatile uint8_t drv8461_diag3_register;
volatile uint8_t drv8461_fault_recheck_register;
volatile uint8_t drv8461_ctrl1_register;
volatile uint8_t drv8461_ctrl2_register;
volatile uint8_t drv8461_status_fault_bits;

volatile GPIO_PinState drv8461_fault_x1_pre_read_level;
volatile uint8_t drv8461_fault_x1_pre_read_asserted;
volatile GPIO_PinState drv8461_fault_x1_post_read_level;
volatile uint8_t drv8461_fault_x1_post_read_asserted;
volatile GPIO_PinState drv8461_fault_x1_level;
volatile uint8_t drv8461_fault_x1_asserted;
volatile uint8_t drv8461_status_headers_valid;
volatile uint8_t drv8461_raw_response_invalid;
volatile uint8_t drv8461_register_defaults_match;
volatile uint8_t drv8461_spi_communication_valid;
volatile uint8_t drv8461_diagnostic_fault_indicated;
volatile uint8_t drv8461_fault_summary_only;
volatile uint8_t drv8461_spi_test_passed;

volatile uint32_t drv8461_wake_tick_ms;
volatile uint32_t drv8461_fault_x1_pre_read_tick_ms;
volatile uint32_t drv8461_fault_initial_tick_ms;
volatile uint32_t drv8461_diag1_tick_ms;
volatile uint32_t drv8461_diag2_tick_ms;
volatile uint32_t drv8461_diag3_tick_ms;
volatile uint32_t drv8461_fault_recheck_delay_start_tick_ms;
volatile uint32_t drv8461_fault_recheck_tick_ms;
volatile uint32_t drv8461_fault_x1_post_read_tick_ms;
volatile uint32_t drv8461_ctrl1_tick_ms;
volatile uint32_t drv8461_ctrl2_tick_ms;
volatile uint32_t drv8461_fault_recheck_elapsed_ms;

static void SPI_Diagnostic_ClearResults(void);
static void SPI_Diagnostic_ForceSafeMotorState(void);
static HAL_StatusTypeDef DRV8461_ReadRegister(uint8_t address,
                                                volatile uint16_t *raw_frame,
                                                volatile uint8_t *status_byte,
                                                volatile uint8_t *register_value);
static void SPI_Diagnostic_Shutdown(void);
static uint8_t DRV8461_IsInvalidRawResponse(uint16_t raw_frame);

/**
 * @brief  Run a read-only U4 diagnostic after the established safe sleep reset.
 * @note   U4 is selected only during HAL_SPI_TransmitReceive(). No write frame
 *         is sent and SLEEP_X is returned LOW before this function exits.
  */
void SPI_Diagnostic_Run(void)
{
  SPI_Diagnostic_ClearResults();
  SPI_Diagnostic_ForceSafeMotorState();
  /* DRV8461 tSLEEP maximum is 120 us; 1 ms guarantees a completed sleep reset. */
  HAL_Delay(1U);

  /* MX_SPI3_Init() runs before this diagnostic in main.c. */
  drv8461_spi_init_status =
      ((hspi3.Instance == SPI3) && (hspi3.State == HAL_SPI_STATE_READY)) ? HAL_OK : HAL_ERROR;
  if (drv8461_spi_init_status == HAL_OK)
  {
    /* SPI-interface tWAKE maximum is 0.25 ms; 2 ms gives conservative margin. */
    HAL_GPIO_WritePin(SLEEP_X_GPIO_Port, SLEEP_X_Pin, GPIO_PIN_SET);
    drv8461_wake_tick_ms = HAL_GetTick();
    HAL_Delay(DRV8461_WAKE_DELAY_MS);

    /* nFAULT is sampled while U4 is awake, before any SPI transfer. */
    drv8461_fault_x1_pre_read_tick_ms = HAL_GetTick();
    drv8461_fault_x1_pre_read_level = HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin);
    drv8461_fault_x1_pre_read_asserted =
        (drv8461_fault_x1_pre_read_level == GPIO_PIN_RESET) ? 1U : 0U;

    drv8461_fault_initial_tick_ms = HAL_GetTick();
    drv8461_fault_read_status = DRV8461_ReadRegister(DRV8461_REG_FAULT,
                                                       &drv8461_fault_raw_frame,
                                                       &drv8461_fault_status_byte,
                                                       &drv8461_fault_register);

    drv8461_diag1_tick_ms = HAL_GetTick();
    drv8461_diag1_read_status = DRV8461_ReadRegister(DRV8461_REG_DIAG1,
                                                       &drv8461_diag1_raw_frame,
                                                       &drv8461_diag1_status_byte,
                                                       &drv8461_diag1_register);
    drv8461_diag2_tick_ms = HAL_GetTick();
    drv8461_diag2_read_status = DRV8461_ReadRegister(DRV8461_REG_DIAG2,
                                                       &drv8461_diag2_raw_frame,
                                                       &drv8461_diag2_status_byte,
                                                       &drv8461_diag2_register);
    drv8461_diag3_tick_ms = HAL_GetTick();
    drv8461_diag3_read_status = DRV8461_ReadRegister(DRV8461_REG_DIAG3,
                                                       &drv8461_diag3_raw_frame,
                                                       &drv8461_diag3_status_byte,
                                                       &drv8461_diag3_register);

    /* Keep U4 awake for the requested observation interval before re-reading FAULT. */
    drv8461_fault_recheck_delay_start_tick_ms = HAL_GetTick();
    HAL_Delay(DRV8461_FAULT_RECHECK_DELAY_MS);
    drv8461_fault_recheck_tick_ms = HAL_GetTick();
    drv8461_fault_recheck_read_status = DRV8461_ReadRegister(DRV8461_REG_FAULT,
                                                               &drv8461_fault_recheck_raw_frame,
                                                               &drv8461_fault_recheck_status_byte,
                                                               &drv8461_fault_recheck_register);
    drv8461_fault_recheck_elapsed_ms =
        drv8461_fault_recheck_tick_ms - drv8461_fault_initial_tick_ms;

    /* This second sample is also while U4 is awake and before SLEEP_X is lowered. */
    drv8461_fault_x1_post_read_tick_ms = HAL_GetTick();
    drv8461_fault_x1_post_read_level = HAL_GPIO_ReadPin(FAULT_X1_GPIO_Port, FAULT_X1_Pin);
    drv8461_fault_x1_post_read_asserted =
        (drv8461_fault_x1_post_read_level == GPIO_PIN_RESET) ? 1U : 0U;
    drv8461_fault_x1_level = drv8461_fault_x1_post_read_level;
    drv8461_fault_x1_asserted = drv8461_fault_x1_post_read_asserted;

    /* Retain CTRL1/CTRL2 reads as independent read-path and safe-default evidence. */
    drv8461_ctrl1_tick_ms = HAL_GetTick();
    drv8461_ctrl1_read_status = DRV8461_ReadRegister(DRV8461_REG_CTRL1,
                                                       &drv8461_ctrl1_raw_frame,
                                                       &drv8461_ctrl1_status_byte,
                                                       &drv8461_ctrl1_register);
    drv8461_ctrl2_tick_ms = HAL_GetTick();
    drv8461_ctrl2_read_status = DRV8461_ReadRegister(DRV8461_REG_CTRL2,
                                                       &drv8461_ctrl2_raw_frame,
                                                       &drv8461_ctrl2_status_byte,
                                                       &drv8461_ctrl2_register);

    drv8461_status_headers_valid =
        (((drv8461_fault_status_byte & DRV8461_STATUS_HEADER_MASK) == DRV8461_STATUS_HEADER_VALUE) &&
         ((drv8461_diag1_status_byte & DRV8461_STATUS_HEADER_MASK) == DRV8461_STATUS_HEADER_VALUE) &&
         ((drv8461_diag2_status_byte & DRV8461_STATUS_HEADER_MASK) == DRV8461_STATUS_HEADER_VALUE) &&
         ((drv8461_diag3_status_byte & DRV8461_STATUS_HEADER_MASK) == DRV8461_STATUS_HEADER_VALUE) &&
         ((drv8461_fault_recheck_status_byte & DRV8461_STATUS_HEADER_MASK) == DRV8461_STATUS_HEADER_VALUE) &&
         ((drv8461_ctrl1_status_byte & DRV8461_STATUS_HEADER_MASK) == DRV8461_STATUS_HEADER_VALUE) &&
         ((drv8461_ctrl2_status_byte & DRV8461_STATUS_HEADER_MASK) == DRV8461_STATUS_HEADER_VALUE)) ? 1U : 0U;

    drv8461_status_fault_bits = (uint8_t)((drv8461_fault_status_byte |
                                            drv8461_diag1_status_byte |
                                            drv8461_diag2_status_byte |
                                            drv8461_diag3_status_byte |
                                            drv8461_fault_recheck_status_byte |
                                            drv8461_ctrl1_status_byte |
                                            drv8461_ctrl2_status_byte) & DRV8461_STATUS_FAULT_MASK);

    drv8461_raw_response_invalid =
        ((DRV8461_IsInvalidRawResponse(drv8461_fault_raw_frame) != 0U) ||
         (DRV8461_IsInvalidRawResponse(drv8461_diag1_raw_frame) != 0U) ||
         (DRV8461_IsInvalidRawResponse(drv8461_diag2_raw_frame) != 0U) ||
         (DRV8461_IsInvalidRawResponse(drv8461_diag3_raw_frame) != 0U) ||
         (DRV8461_IsInvalidRawResponse(drv8461_fault_recheck_raw_frame) != 0U) ||
         (DRV8461_IsInvalidRawResponse(drv8461_ctrl1_raw_frame) != 0U) ||
         (DRV8461_IsInvalidRawResponse(drv8461_ctrl2_raw_frame) != 0U)) ? 1U : 0U;

    drv8461_register_defaults_match =
        ((drv8461_fault_register == 0x00U) &&
         (drv8461_fault_recheck_register == 0x00U) &&
         (drv8461_ctrl1_register == DRV8461_CTRL1_RESET_VALUE) &&
         (drv8461_ctrl2_register == DRV8461_CTRL2_RESET_VALUE)) ? 1U : 0U;

    drv8461_spi_communication_valid =
        ((drv8461_fault_read_status == HAL_OK) &&
         (drv8461_diag1_read_status == HAL_OK) &&
         (drv8461_diag2_read_status == HAL_OK) &&
         (drv8461_diag3_read_status == HAL_OK) &&
         (drv8461_fault_recheck_read_status == HAL_OK) &&
         (drv8461_ctrl1_read_status == HAL_OK) &&
         (drv8461_ctrl2_read_status == HAL_OK) &&
         (drv8461_status_headers_valid != 0U) &&
         (drv8461_raw_response_invalid == 0U)) ? 1U : 0U;

    drv8461_diagnostic_fault_indicated =
        ((drv8461_fault_register != 0x00U) ||
         (drv8461_fault_recheck_register != 0x00U) ||
         (drv8461_status_fault_bits != 0U) ||
         (drv8461_fault_x1_pre_read_asserted != 0U) ||
         (drv8461_fault_x1_post_read_asserted != 0U)) ? 1U : 0U;

    drv8461_fault_summary_only =
        ((drv8461_fault_register == 0x80U) &&
         (drv8461_fault_recheck_register == 0x80U) &&
         (drv8461_status_fault_bits == 0U) &&
         (drv8461_fault_x1_pre_read_asserted == 0U) &&
         (drv8461_fault_x1_post_read_asserted == 0U)) ? 1U : 0U;

    drv8461_spi_test_passed =
        ((drv8461_spi_communication_valid != 0U) &&
         (drv8461_diagnostic_fault_indicated == 0U) &&
         (drv8461_register_defaults_match != 0U)) ? 1U : 0U;
  }

  SPI_Diagnostic_Shutdown();
}

/**
 * @brief  Show a rapid three-flash burst followed by a long pause for SPI PASS.
  */
void SPI_Diagnostic_IndicatePass(void)
{
  uint8_t flash_count;

  for (flash_count = 0U; flash_count < 3U; ++flash_count)
  {
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
    HAL_Delay(DRV8461_LED_PASS_FLASH_MS);
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
    HAL_Delay(DRV8461_LED_PASS_FLASH_MS);
  }

  HAL_Delay(DRV8461_LED_PATTERN_PAUSE_MS);
}

/**
 * @brief  Show two medium flashes when SPI communication worked but data is abnormal.
 */
void SPI_Diagnostic_IndicateDiagnosticIssue(void)
{
  uint8_t flash_count;

  for (flash_count = 0U; flash_count < 2U; ++flash_count)
  {
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
    HAL_Delay(DRV8461_LED_ISSUE_ON_MS);
    HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
    HAL_Delay(DRV8461_LED_ISSUE_OFF_MS);
  }

  HAL_Delay(DRV8461_LED_PATTERN_PAUSE_MS);
}

/**
 * @brief  Show one clearly long flash followed by a long pause for SPI FAIL.
  */
void SPI_Diagnostic_IndicateFail(void)
{
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_SET);
  HAL_Delay(DRV8461_LED_FAIL_ON_MS);
  HAL_GPIO_WritePin(STATUS_RUN_GPIO_Port, STATUS_RUN_Pin, GPIO_PIN_RESET);
  HAL_Delay(DRV8461_LED_PATTERN_PAUSE_MS);
}

static void SPI_Diagnostic_ClearResults(void)
{
  drv8461_spi_init_status = HAL_ERROR;
  drv8461_fault_read_status = HAL_ERROR;
  drv8461_diag1_read_status = HAL_ERROR;
  drv8461_diag2_read_status = HAL_ERROR;
  drv8461_diag3_read_status = HAL_ERROR;
  drv8461_fault_recheck_read_status = HAL_ERROR;
  drv8461_ctrl1_read_status = HAL_ERROR;
  drv8461_ctrl2_read_status = HAL_ERROR;
  drv8461_fault_raw_frame = 0U;
  drv8461_diag1_raw_frame = 0U;
  drv8461_diag2_raw_frame = 0U;
  drv8461_diag3_raw_frame = 0U;
  drv8461_fault_recheck_raw_frame = 0U;
  drv8461_ctrl1_raw_frame = 0U;
  drv8461_ctrl2_raw_frame = 0U;
  drv8461_fault_status_byte = 0U;
  drv8461_diag1_status_byte = 0U;
  drv8461_diag2_status_byte = 0U;
  drv8461_diag3_status_byte = 0U;
  drv8461_fault_recheck_status_byte = 0U;
  drv8461_ctrl1_status_byte = 0U;
  drv8461_ctrl2_status_byte = 0U;
  drv8461_fault_register = 0U;
  drv8461_diag1_register = 0U;
  drv8461_diag2_register = 0U;
  drv8461_diag3_register = 0U;
  drv8461_fault_recheck_register = 0U;
  drv8461_ctrl1_register = 0U;
  drv8461_ctrl2_register = 0U;
  drv8461_status_fault_bits = 0U;
  drv8461_fault_x1_pre_read_level = GPIO_PIN_RESET;
  drv8461_fault_x1_pre_read_asserted = 1U;
  drv8461_fault_x1_post_read_level = GPIO_PIN_RESET;
  drv8461_fault_x1_post_read_asserted = 1U;
  drv8461_fault_x1_level = GPIO_PIN_RESET;
  drv8461_fault_x1_asserted = 1U;
  drv8461_status_headers_valid = 0U;
  drv8461_raw_response_invalid = 1U;
  drv8461_register_defaults_match = 0U;
  drv8461_spi_communication_valid = 0U;
  drv8461_diagnostic_fault_indicated = 0U;
  drv8461_fault_summary_only = 0U;
  drv8461_spi_test_passed = 0U;
  drv8461_wake_tick_ms = 0U;
  drv8461_fault_x1_pre_read_tick_ms = 0U;
  drv8461_fault_initial_tick_ms = 0U;
  drv8461_diag1_tick_ms = 0U;
  drv8461_diag2_tick_ms = 0U;
  drv8461_diag3_tick_ms = 0U;
  drv8461_fault_recheck_delay_start_tick_ms = 0U;
  drv8461_fault_recheck_tick_ms = 0U;
  drv8461_fault_x1_post_read_tick_ms = 0U;
  drv8461_ctrl1_tick_ms = 0U;
  drv8461_ctrl2_tick_ms = 0U;
  drv8461_fault_recheck_elapsed_ms = 0U;
}

static void SPI_Diagnostic_ForceSafeMotorState(void)
{
  HAL_GPIO_WritePin(CS_X1_GPIO_Port, CS_X1_Pin | CS_X2_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Y_GPIO_Port, CS_Y_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Z_GPIO_Port, CS_Z_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(SLEEP_X_GPIO_Port, SLEEP_X_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Y_MCU_GPIO_Port, ENA_Y_MCU_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ENA_Z_MCU_GPIO_Port, ENA_Z_MCU_Pin, GPIO_PIN_RESET);
}

static HAL_StatusTypeDef DRV8461_ReadRegister(uint8_t address,
                                                volatile uint16_t *raw_frame,
                                                volatile uint8_t *status_byte,
                                                volatile uint8_t *register_value)
{
  uint8_t tx_data[2] = {(uint8_t)(DRV8461_READ_COMMAND_BIT | (address & 0x3FU)), 0x00U};
  uint8_t rx_data[2] = {0U, 0U};
  HAL_StatusTypeDef transfer_status;

  HAL_GPIO_WritePin(CS_X1_GPIO_Port, CS_X1_Pin, GPIO_PIN_RESET);
  transfer_status = HAL_SPI_TransmitReceive(&hspi3, tx_data, rx_data, 2U, DRV8461_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(CS_X1_GPIO_Port, CS_X1_Pin, GPIO_PIN_SET);
  HAL_Delay(DRV8461_FRAME_GAP_MS);

  *raw_frame = ((uint16_t)rx_data[0] << 8U) | rx_data[1];
  *status_byte = rx_data[0];
  *register_value = rx_data[1];

  return transfer_status;
}

static void SPI_Diagnostic_Shutdown(void)
{
  HAL_GPIO_WritePin(CS_X1_GPIO_Port, CS_X1_Pin | CS_X2_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Y_GPIO_Port, CS_Y_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CS_Z_GPIO_Port, CS_Z_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(SLEEP_X_GPIO_Port, SLEEP_X_Pin, GPIO_PIN_RESET);
  SPI_Diagnostic_ForceSafeMotorState();

  /* SPI3 remains in its CubeMX-generated idle configuration; no further transfer is issued. */
}

static uint8_t DRV8461_IsInvalidRawResponse(uint16_t raw_frame)
{
  return ((raw_frame == 0x0000U) || (raw_frame == 0xFFFFU)) ? 1U : 0U;
}
