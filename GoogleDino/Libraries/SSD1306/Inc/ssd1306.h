/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SSD1306_H
#define SSD1306_H

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include "py32f0xx_hal.h" // IWYU pragma: keep

/* Private defines -----------------------------------------------------------*/
// Constants
#define SSD1306_WIDTH		128U
#define SSD1306_HEIGHT		64U
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

#define SSD1306_MAX_X	(SSD1306_WIDTH - 1)
#define SSD1306_MAX_Y	(SSD1306_HEIGHT - 1)
#define SSD1306_MAX_ROW (8 - 1)

// Display control
#define SSD1306_DISPLAYON			0xAF
#define SSD1306_DISPLAYOFF			0xAE
#define SSD1306_DISPLAYALLON_RESUME 0xA4

// Display direction
#define SSD1306_NORMAL_V 0xC8
#define SSD1306_FLIP_V	 0xC0
#define SSD1306_NORMAL_H 0xA1
#define SSD1306_FLIP_H	 0xA0

// Display mode
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7

// Memory addressing
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR   0x22

// Display configuration
#define SSD1306_SETSTARTLINE	   0x40
#define SSD1306_SETCONTRAST		   0x81
#define SSD1306_CHARGEPUMP		   0x8D
#define SSD1306_SETMULTIPLEX	   0xA8
#define SSD1306_SETDISPLAYOFFSET   0xD3
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE	   0xD9
#define SSD1306_SETCOMPINS		   0xDA
#define SSD1306_SETVCOMDETECT	   0xDB

#define SSD1306_DEACTIVATE_SCROLL 0x2E

typedef struct
{
	SPI_HandleTypeDef *SPI;

	GPIO_TypeDef *CSPort;
	uint16_t	  CSPin;

	GPIO_TypeDef *DCPort;
	uint16_t	  DCPin;

	GPIO_TypeDef *RSTPort;
	uint16_t	  RSTPin;

} SSD1306_InitTypeDef;

/* Exported functions prototypes ---------------------------------------------*/
HAL_StatusTypeDef SSD1306_Init(SSD1306_InitTypeDef *SSD1306_Init);
HAL_StatusTypeDef SSD1306_Display(void);
HAL_StatusTypeDef SSD1306_DisplayPart(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);

HAL_StatusTypeDef SSD1306_SetOnOff(bool on);
HAL_StatusTypeDef SSD1306_SetContrast(uint8_t contrast);
HAL_StatusTypeDef SSD1306_SetInverted(bool inverted);
HAL_StatusTypeDef SSD1306_FlipH(bool mode);
HAL_StatusTypeDef SSD1306_FlipV(bool mode);

void SSD1306_SetPixel(uint8_t x, uint8_t y, bool color);

void SSD1306_DrawFastHLine(uint8_t x, uint8_t y, uint8_t width, bool color);
void SSD1306_DrawFastVLine(uint8_t x, uint8_t y, uint8_t height, bool color);
void SSD1306_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool color);

void SSD1306_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool color, bool filled);
void SSD1306_DrawRoundRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t radius, bool color, bool filled);
void SSD1306_DrawTriangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool color, bool filled);
void SSD1306_DrawCircle(uint8_t x0, uint8_t y0, uint8_t radius, bool color, bool filled);

void SSD1306_DrawBitmap(uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t width, uint8_t height, bool color);

void SSD1306_Home(void);
void SSD1306_SetCursor(uint8_t x, uint8_t y);
void SSD1306_SetScale(uint8_t scale);
void SSD1306_SetAutoPrintln(bool enabled);

void SSD1306_DrawByte(uint8_t data, bool color);
void SSD1306_Print(const char *text, bool color);
void SSD1306_Println(const char *text, bool color);

void SSD1306_Clear(bool color);
void SSD1306_ClearPart(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool color);

uint8_t *SSD1306_GetBuffer(void);

#endif // SSD1306_H
