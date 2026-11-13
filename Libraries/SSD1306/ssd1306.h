/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SSD1306_H
#define __SSD1306_H

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <string.h>
#include "py32f0xx_hal.h"

/* Private defines -----------------------------------------------------------*/
#define SSD1306_WIDTH 128U
#define SSD1306_HEIGHT 64U
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR 0x22
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_SEGREMAP 0xA1
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_COMSCAN 0xC8
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETSTARTLINE 0x40

// TODO: Will be used in the future (rows with tabs are not used yet)
	#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26
	#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27
	#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
	#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A
#define SSD1306_DEACTIVATE_SCROLL 0x2E
	#define SSD1306_ACTIVATE_SCROLL 0x2F
	#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3

typedef struct
{
	SPI_HandleTypeDef *SPI;

	GPIO_TypeDef *CSPort;
	uint16_t CSPin;

	GPIO_TypeDef *DCPort;
	uint16_t DCPin;

	GPIO_TypeDef *RSTPort;
	uint16_t RSTPin;

	uint32_t SPITimeoutMs;

	uint8_t Contrast;
	bool Inverted;

} SSD1306_InitTypeDef;

/* Exported functions prototypes ---------------------------------------------*/
HAL_StatusTypeDef SSD1306_Init(SSD1306_InitTypeDef *SSD1306_Init);
HAL_StatusTypeDef SSD1306_Display(void);
HAL_StatusTypeDef SSD1306_SetContrast(uint8_t contrast);

// TODO: Print text
// TODO: Draw rectange
// TODO: Draw bitmap
void SSD1306_Clear(bool color);

uint8_t *SSD1306_GetBuffer(void);

#endif // __SSD1306_H