/**
 ******************************************************************************
 * @file    main.c
 * @author  MCU Application Team
 * @brief   Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2023 Puya Semiconductor Co.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by Puya under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ssd1306.h"

/* Private define ------------------------------------------------------------*/
#define OLED_CS_GPIO_PORT     GPIOA
#define OLED_CS_PIN           GPIO_PIN_6
#define OLED_DC_GPIO_PORT     GPIOA
#define OLED_DC_PIN           GPIO_PIN_4
#define OLED_RESET_GPIO_PORT  GPIOB
#define OLED_RESET_PIN        GPIO_PIN_1

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef Spi1Handle;

/* Private user code ---------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void APP_SPI1_Init(void);
static void APP_SSD1306_Init(void);

/**
 * @brief  Main program.
 * @retval int
 */
int main(void)
{
  /* Reset of all peripherals, Initializes the Systick. */
  HAL_Init();

  /* System clock configuration */
  APP_SystemClockConfig();

  APP_SPI1_Init();

  APP_SSD1306_Init();

  /* infinite loop */
  while (1)
  {
  }
}

/**
 * @brief  System clock configuration function
 * @param  None
 * @retval None
 */
static void APP_SystemClockConfig(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* Oscillator configuration */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_LSE; /* Select oscillator HSE, HSI, LSI, LSE */
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;                                                                                              /* Enable HSI */
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;                                                                                              /* HSI 1 frequency division */
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;                                                                     /* Configure HSI clock 24MHz */
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS_DISABLE;                                                                                  /* Close HSE bypass */
  RCC_OscInitStruct.LSIState = RCC_LSI_OFF;                                                                                             /* Close LSI */
  /*RCC_OscInitStruct.LSICalibrationValue = RCC_LSICALIBRATION_32768Hz;*/
  RCC_OscInitStruct.LSEState = RCC_LSE_OFF; /* Close LSE */
  /*RCC_OscInitStruct.LSEDriver = RCC_LSEDRIVE_MEDIUM;*/
  /* Configure oscillator */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Clock source configuration */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1; /* Choose to configure clock HCLK, SYSCLK, PCLK1 */
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSISYS;                                      /* Select HSISYS as the system clock */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;                                             /* AHB clock 1 division */
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;                                              /* APB clock 1 division */
  /* Configure clock source */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

static void APP_SPI1_Init(void)
{
  Spi1Handle.Instance = SPI1;                                  /* SPI1 */
  Spi1Handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; /* 4 frequency division */
  Spi1Handle.Init.Direction = SPI_DIRECTION_2LINES;            /* full duplex */
  Spi1Handle.Init.CLKPolarity = SPI_POLARITY_LOW;              /* Low clock polarity */
  Spi1Handle.Init.CLKPhase = SPI_PHASE_1EDGE;                  /* Data sampling starts from the first clock edge */
  Spi1Handle.Init.DataSize = SPI_DATASIZE_8BIT;                /* SPI data length is 8 bits */
  Spi1Handle.Init.FirstBit = SPI_FIRSTBIT_MSB;                 /* Send MSB first */
  Spi1Handle.Init.NSS = SPI_NSS_SOFT;                          /* NSS Software Mode */
  Spi1Handle.Init.Mode = SPI_MODE_MASTER;                      /* Configure as master */

  /* De-Initialize the SPI peripheral */
  if (HAL_SPI_DeInit(&Spi1Handle) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Initialize SPI peripheral */
  if (HAL_SPI_Init(&Spi1Handle) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

static void APP_SSD1306_Init(void)
{
  SSD1306_InitTypeDef SSD1306_InitStruct = {};

  SSD1306_InitStruct.SPI = &Spi1Handle;

  SSD1306_InitStruct.CSPort = OLED_CS_GPIO_PORT;
  SSD1306_InitStruct.CSPin = OLED_CS_PIN;

  SSD1306_InitStruct.DCPort = OLED_DC_GPIO_PORT;
  SSD1306_InitStruct.DCPin = OLED_DC_PIN;

  SSD1306_InitStruct.RSTPort = OLED_RESET_GPIO_PORT;
  SSD1306_InitStruct.RSTPin = OLED_RESET_PIN;

  SSD1306_InitStruct.SPITimeoutMs = 100;

  SSD1306_InitStruct.Contrast = 127;
  SSD1306_InitStruct.Inverted = false;

  if (SSD1306_Init(&SSD1306_InitStruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

/**
 * @brief  Error executing function.
 * @param  None
 * @retval None
 */
void APP_ErrorHandler(void)
{
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* Users can add their own printing information as needed,
     for example: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* Infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
