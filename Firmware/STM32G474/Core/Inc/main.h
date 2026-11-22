/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DIR_X_Pin GPIO_PIN_2
#define DIR_X_GPIO_Port GPIOE
#define STEP_X_Pin GPIO_PIN_3
#define STEP_X_GPIO_Port GPIOE
#define CS_X1_Pin GPIO_PIN_4
#define CS_X1_GPIO_Port GPIOE
#define CS_X2_Pin GPIO_PIN_5
#define CS_X2_GPIO_Port GPIOE
#define SLEEP_X_Pin GPIO_PIN_6
#define SLEEP_X_GPIO_Port GPIOE
#define FAULT_X2_Pin GPIO_PIN_13
#define FAULT_X2_GPIO_Port GPIOC
#define ENA_X_MCU_Pin GPIO_PIN_9
#define ENA_X_MCU_GPIO_Port GPIOF
#define FAULT_X1_Pin GPIO_PIN_10
#define FAULT_X1_GPIO_Port GPIOF
#define ENA_Y_MCU_Pin GPIO_PIN_3
#define ENA_Y_MCU_GPIO_Port GPIOC
#define MX1_ENC_B_Pin GPIO_PIN_0
#define MX1_ENC_B_GPIO_Port GPIOA
#define MX1_ENC_A_Pin GPIO_PIN_1
#define MX1_ENC_A_GPIO_Port GPIOA
#define MX1_ENC_Z_Pin GPIO_PIN_2
#define MX1_ENC_Z_GPIO_Port GPIOA
#define MX1_ENC_Z_EXTI_IRQn EXTI2_IRQn
#define ENA_Z_MCU_Pin GPIO_PIN_3
#define ENA_Z_MCU_GPIO_Port GPIOA
#define STATUS_RUN_Pin GPIO_PIN_12
#define STATUS_RUN_GPIO_Port GPIOD
#define SLEEP_Z_Pin GPIO_PIN_15
#define SLEEP_Z_GPIO_Port GPIOD
#define FAULT_Z_Pin GPIO_PIN_8
#define FAULT_Z_GPIO_Port GPIOC
#define CS_Z_Pin GPIO_PIN_15
#define CS_Z_GPIO_Port GPIOA
#define SPI_SCLK_Pin GPIO_PIN_10
#define SPI_SCLK_GPIO_Port GPIOC
#define SPI_DIO_Pin GPIO_PIN_11
#define SPI_DIO_GPIO_Port GPIOC
#define SPI_SDI_Pin GPIO_PIN_12
#define SPI_SDI_GPIO_Port GPIOC
#define CS_Y_Pin GPIO_PIN_0
#define CS_Y_GPIO_Port GPIOD
#define FAULT_Y_Pin GPIO_PIN_6
#define FAULT_Y_GPIO_Port GPIOD
#define SLEEP_Y_Pin GPIO_PIN_7
#define SLEEP_Y_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
