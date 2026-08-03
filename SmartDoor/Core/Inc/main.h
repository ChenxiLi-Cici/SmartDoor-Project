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
#include "stm32f3xx_hal.h"

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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define SR_SRCLK_Pin GPIO_PIN_0
#define SR_SRCLK_GPIO_Port GPIOC
#define SW4_Pin GPIO_PIN_1
#define SW4_GPIO_Port GPIOC
#define Buzzer_Pin GPIO_PIN_2
#define Buzzer_GPIO_Port GPIOC
#define SW1_Pin GPIO_PIN_1
#define SW1_GPIO_Port GPIOA
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define SW2_Pin GPIO_PIN_4
#define SW2_GPIO_Port GPIOA
#define LD2_Pin GPIO_PIN_5
#define LD2_GPIO_Port GPIOA
#define LDR1_Pin GPIO_PIN_4
#define LDR1_GPIO_Port GPIOC
#define COIL_A_Pin GPIO_PIN_5
#define COIL_A_GPIO_Port GPIOC
#define SW3_Pin GPIO_PIN_0
#define SW3_GPIO_Port GPIOB
#define LDR2_Pin GPIO_PIN_1
#define LDR2_GPIO_Port GPIOB
#define COIL_D_Pin GPIO_PIN_2
#define COIL_D_GPIO_Port GPIOB
#define LED_D1_Pin GPIO_PIN_10
#define LED_D1_GPIO_Port GPIOB
#define ROW1_Pin GPIO_PIN_11
#define ROW1_GPIO_Port GPIOB
#define ROW2_Pin GPIO_PIN_12
#define ROW2_GPIO_Port GPIOB
#define ROW3_Pin GPIO_PIN_13
#define ROW3_GPIO_Port GPIOB
#define ROW4_Pin GPIO_PIN_14
#define ROW4_GPIO_Port GPIOB
#define SR_SER_Pin GPIO_PIN_15
#define SR_SER_GPIO_Port GPIOB
#define LCD_RW_Pin GPIO_PIN_6
#define LCD_RW_GPIO_Port GPIOC
#define COIL_C_Pin GPIO_PIN_7
#define COIL_C_GPIO_Port GPIOC
#define LCD_D4_Pin GPIO_PIN_8
#define LCD_D4_GPIO_Port GPIOC
#define LCD_D5_Pin GPIO_PIN_9
#define LCD_D5_GPIO_Port GPIOC
#define COL1_Pin GPIO_PIN_8
#define COL1_GPIO_Port GPIOA
#define COL2_Pin GPIO_PIN_9
#define COL2_GPIO_Port GPIOA
#define COL3_Pin GPIO_PIN_10
#define COL3_GPIO_Port GPIOA
#define COL4_Pin GPIO_PIN_11
#define COL4_GPIO_Port GPIOA
#define COIL_B_Pin GPIO_PIN_12
#define COIL_B_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define LCD_RS_Pin GPIO_PIN_15
#define LCD_RS_GPIO_Port GPIOA
#define LCD_D6_Pin GPIO_PIN_10
#define LCD_D6_GPIO_Port GPIOC
#define LCD_D7_Pin GPIO_PIN_11
#define LCD_D7_GPIO_Port GPIOC
#define LCD_E_Pin GPIO_PIN_2
#define LCD_E_GPIO_Port GPIOD
#define LED_D4_Pin GPIO_PIN_3
#define LED_D4_GPIO_Port GPIOB
#define LED_D2_Pin GPIO_PIN_4
#define LED_D2_GPIO_Port GPIOB
#define LED_D3_Pin GPIO_PIN_5
#define LED_D3_GPIO_Port GPIOB
#define SR_RCLK_Pin GPIO_PIN_7
#define SR_RCLK_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
