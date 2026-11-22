/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, DIR_X_Pin|STEP_X_Pin|SLEEP_X_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, CS_X1_Pin|CS_X2_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ENA_X_MCU_GPIO_Port, ENA_X_MCU_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ENA_Y_MCU_GPIO_Port, ENA_Y_MCU_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ENA_Z_MCU_GPIO_Port, ENA_Z_MCU_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, STATUS_RUN_Pin|SLEEP_Z_Pin|SLEEP_Y_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_Z_GPIO_Port, CS_Z_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_Y_GPIO_Port, CS_Y_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : DIR_X_Pin STEP_X_Pin CS_X1_Pin CS_X2_Pin
                           SLEEP_X_Pin */
  GPIO_InitStruct.Pin = DIR_X_Pin|STEP_X_Pin|CS_X1_Pin|CS_X2_Pin
                          |SLEEP_X_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : FAULT_X2_Pin FAULT_Z_Pin */
  GPIO_InitStruct.Pin = FAULT_X2_Pin|FAULT_Z_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : ENA_X_MCU_Pin */
  GPIO_InitStruct.Pin = ENA_X_MCU_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ENA_X_MCU_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : FAULT_X1_Pin */
  GPIO_InitStruct.Pin = FAULT_X1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(FAULT_X1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ENA_Y_MCU_Pin */
  GPIO_InitStruct.Pin = ENA_Y_MCU_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ENA_Y_MCU_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MX1_ENC_Z_Pin */
  GPIO_InitStruct.Pin = MX1_ENC_Z_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MX1_ENC_Z_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ENA_Z_MCU_Pin CS_Z_Pin */
  GPIO_InitStruct.Pin = ENA_Z_MCU_Pin|CS_Z_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : STATUS_RUN_Pin SLEEP_Z_Pin CS_Y_Pin SLEEP_Y_Pin */
  GPIO_InitStruct.Pin = STATUS_RUN_Pin|SLEEP_Z_Pin|CS_Y_Pin|SLEEP_Y_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : FAULT_Y_Pin */
  GPIO_InitStruct.Pin = FAULT_Y_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(FAULT_Y_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
