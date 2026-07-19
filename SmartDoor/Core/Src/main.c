/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* PN532 的 7 位 I2C 地址为 0x24，HAL 需要 8 位格式，故左移一位得 0x48 */
#define PN532_I2C_ADDRESS       (0x24 << 1)

/* PN532 帧固定字段 */
#define PN532_PREAMBLE          0x00
#define PN532_STARTCODE1        0x00
#define PN532_STARTCODE2        0xFF
#define PN532_HOSTTOPN532       0xD4    /* 主机 -> PN532 方向标识 */
#define PN532_PN532TOHOST       0xD5    /* PN532 -> 主机 方向标识 */

/* PN532 指令码 */
#define PN532_CMD_SAMCONFIG     0x14
#define PN532_CMD_INLISTPASSIVE 0x4A

/* I2C 读操作时，PN532 返回的第一个字节为就绪状态字 */
#define PN532_READY             0x01

/* 通用超时与重试参数 */
#define PN532_I2C_TIMEOUT       100     /* 单次 I2C 传输超时 (ms) */
#define PN532_READY_RETRIES     30      /* 轮询就绪状态的最大次数 */
#define PN532_READY_INTERVAL    10      /* 每次轮询之间的间隔 (ms) */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC2_Init(void);
static void MX_ADC3_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* PN532 底层驱动与接口函数实现 */
void     PN532_Wakeup(void);
uint8_t  PN532_SAMConfig(void);
int      PN532_Get_UID(uint8_t *uid, uint8_t *uidLen);

static uint8_t PN532_ComputeDataChecksum(const uint8_t *data, uint8_t len);
static HAL_StatusTypeDef PN532_SendCommand(const uint8_t *cmd, uint8_t cmdLen);
static HAL_StatusTypeDef PN532_WaitReady(uint32_t retries);
static HAL_StatusTypeDef PN532_ReadResponse(uint8_t *buf, uint16_t len);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#ifdef __GNUC__
/**
  * @brief  重定向 printf 到 USART2（板载 ST-Link 虚拟串口），适配 GCC 工具链
  */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, 0xFFFF);
    return len;
}
#endif

/**
  * @brief  发送前导字节physically唤醒 PN532
  */
void PN532_Wakeup(void)
{
    uint8_t wake[] = { 0x55, 0x55, 0x00, 0x00, 0x00 };
    HAL_I2C_Master_Transmit(&hi2c1, PN532_I2C_ADDRESS, wake, sizeof(wake), PN532_I2C_TIMEOUT);
    HAL_Delay(10);
}

/**
  * @brief  计算数据校验和 DCS
  */
static uint8_t PN532_ComputeDataChecksum(const uint8_t *data, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        sum = (uint8_t)(sum + data[i]);
    }
    return (uint8_t)(~sum + 1);
}

/**
  * @brief  按 PN532 普通信息帧格式打包并发送一条指令
  */
static HAL_StatusTypeDef PN532_SendCommand(const uint8_t *cmd, uint8_t cmdLen)
{
    uint8_t frame[32];
    uint8_t data[32];
    uint8_t len = (uint8_t)(cmdLen + 1);
    uint8_t idx = 0;

    if (cmdLen + 8 > (uint8_t)sizeof(frame))
    {
        return HAL_ERROR;
    }

    data[0] = PN532_HOSTTOPN532;
    memcpy(&data[1], cmd, cmdLen);

    frame[idx++] = PN532_PREAMBLE;
    frame[idx++] = PN532_STARTCODE1;
    frame[idx++] = PN532_STARTCODE2;
    frame[idx++] = len;
    frame[idx++] = (uint8_t)(~len + 1);

    memcpy(&frame[idx], data, len);
    idx = (uint8_t)(idx + len);

    frame[idx++] = PN532_ComputeDataChecksum(data, len);
    frame[idx++] = PN532_PREAMBLE;

    return HAL_I2C_Master_Transmit(&hi2c1, PN532_I2C_ADDRESS, frame, idx, PN532_I2C_TIMEOUT);
}

/**
  * @brief  轮询 PN532 的就绪状态字
  */
static HAL_StatusTypeDef PN532_WaitReady(uint32_t retries)
{
    uint8_t ready = 0;
    for (uint32_t i = 0; i < retries; i++)
    {
        if (HAL_I2C_Master_Receive(&hi2c1, PN532_I2C_ADDRESS, &ready, 1, PN532_I2C_TIMEOUT) == HAL_OK)
        {
            if (ready == PN532_READY)
            {
                return HAL_OK;
            }
        }
        HAL_Delay(PN532_READY_INTERVAL);
    }
    return HAL_TIMEOUT;
}

/**
  * @brief  读取一帧响应（含首位就绪状态字）
  */
static HAL_StatusTypeDef PN532_ReadResponse(uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Master_Receive(&hi2c1, PN532_I2C_ADDRESS, buf, len, PN532_I2C_TIMEOUT);
}

/**
  * @brief  配置 PN532 的 SAM 为普通读卡模式
  * @retval 1: 配置成功, 0: 配置失败
  */
uint8_t PN532_SAMConfig(void)
{
    uint8_t cmd[] = { PN532_CMD_SAMCONFIG, 0x01, 0x14, 0x01 };
    uint8_t buf[16] = { 0 };

    if (PN532_SendCommand(cmd, sizeof(cmd)) != HAL_OK)
    {
        return 0;
    }

    if (PN532_WaitReady(PN532_READY_RETRIES) != HAL_OK)
    {
        return 0;
    }

    if (PN532_ReadResponse(buf, 7) != HAL_OK)
    {
        return 0;
    }

    if (PN532_WaitReady(PN532_READY_RETRIES) == HAL_OK)
    {
        PN532_ReadResponse(buf, 9);
    }

    return 1;
}

/**
  * @brief  非阻塞寻卡获取目标 UID 的核心函数接口（供业务或业务轮询调用）
  * @param  uid    用于存储输出 UID 的缓冲区
  * @param  uidLen 用于存储输出 UID 实际长度的变量指针
  * @retval 1 表示成功读到卡片且获取了 UID，0 表示本次未检测到卡片
  */
int PN532_Get_UID(uint8_t *uid, uint8_t *uidLen)
{
    uint8_t cmd[] = { PN532_CMD_INLISTPASSIVE, 0x01, 0x00 };
    uint8_t buf[32] = { 0 };
    uint8_t len;

    if (PN532_SendCommand(cmd, sizeof(cmd)) != HAL_OK)
    {
        return 0;
    }

    if (PN532_WaitReady(PN532_READY_RETRIES) != HAL_OK)
    {
        return 0;
    }
    PN532_ReadResponse(buf, 7);

    if (PN532_WaitReady(PN532_READY_RETRIES) != HAL_OK)
    {
        return 0;
    }

    if (PN532_ReadResponse(buf, sizeof(buf)) != HAL_OK)
    {
        return 0;
    }

    if (buf[0] != PN532_READY || buf[1] != 0x00 || buf[2] != 0x00 || buf[3] != 0xFF)
    {
        return 0;
    }

    if (buf[6] != PN532_PN532TOHOST || buf[7] != (PN532_CMD_INLISTPASSIVE + 1))
    {
        return 0;
    }

    if (buf[8] == 0)
    {
        return 0;
    }

    len = buf[13];
    if (len == 0 || len > 7)
    {
        return 0;
    }

    memcpy(uid, &buf[14], len);
    *uidLen = len;

    return 1;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  // Start the 1ms timer
  HAL_TIM_Base_Start_IT(&htim2);

  printf("\r\n=== Smart Door Init ===\r\n");

  /* Wake up and configure the PN532 module */
  PN532_Wakeup();
  if (PN532_SAMConfig()) {
      printf(" PN532 NFC Module Initialised Successfully.\r\n");
  } else {
      printf("WARN: PN532 Configuration Failed.\r\n");
  }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // Constantly refresh the non-blocking timers
	  io_actuators_process();
	  // call FSM

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_ADC12|RCC_PERIPHCLK_ADC34
                              |RCC_PERIPHCLK_TIM2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  PeriphClkInit.Adc34ClockSelection = RCC_ADC34PLLCLK_DIV1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.Tim2ClockSelection = RCC_TIM2CLK_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{
  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DMAContinuousRequests = DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc3, &multimode) != HAL_OK)
  {
    Error_Handler();
  }
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00201D2B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, SR_SRCLK_Pin|Buzzer_Pin|COIL_A_Pin|LCD_RW_Pin
                          |COIL_C_Pin|LCD_D4_Pin|LCD_D5_Pin|LCD_D6_Pin
                          |LCD_D7_Pin, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(GPIOA, LD2_Pin|COIL_B_Pin|LCD_RS_Pin, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(GPIOB, COIL_D_Pin|LED_D1_Pin|ROW1_Pin|ROW2_Pin
                          |ROW3_Pin|ROW4_Pin|SR_SER_Pin|LED_D4_Pin
                          |LED_D2_Pin|LED_D3_Pin|SR_RCLK_Pin, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SR_SRCLK_Pin|Buzzer_Pin|COIL_A_Pin|LCD_RW_Pin
                          |COIL_C_Pin|LCD_D4_Pin|LCD_D5_Pin|LCD_D6_Pin
                          |LCD_D7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SW4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SW4_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SW1_Pin|SW2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LD2_Pin|COIL_B_Pin|LCD_RS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SW3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SW3_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = COIL_D_Pin|LED_D1_Pin|ROW1_Pin|ROW2_Pin
                          |ROW3_Pin|ROW4_Pin|SR_SER_Pin|LED_D4_Pin
                          |LED_D2_Pin|LED_D3_Pin|SR_RCLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = COL1_Pin|COL2_Pin|COL3_Pin|COL4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_E_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_E_GPIO_Port, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }


}

/* USER CODE BEGIN 4 */

/* Import variables from output file */
extern volatile uint32_t led_blink_timeout;
extern volatile uint32_t led_blink_counter;
extern volatile uint32_t buzzer_timeout;


// Period elapsed callback
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
      if (led_blink_timeout > 0)  led_blink_timeout--;
      if (led_blink_counter > 0)  led_blink_counter--;
      if (buzzer_timeout > 0)     buzzer_timeout--;
  }
}

/* USER CODE END 4 */
