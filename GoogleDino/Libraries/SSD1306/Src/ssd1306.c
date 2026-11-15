/* Includes ------------------------------------------------------------------*/
#include "ssd1306.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#ifndef _swap_int8_t
#define _swap_int8_t(a, b) \
	{                      \
		int8_t t = a;      \
		a		 = b;      \
		b		 = t;      \
	}
#endif

/* Private variables ---------------------------------------------------------*/
static SSD1306_InitTypeDef display;

static uint8_t buffer[SSD1306_BUFFER_SIZE];

/* Private macro -------------------------------------------------------------*/
#define SSD1306_SELECT		 HAL_GPIO_WritePin(display.CSPort, display.CSPin, GPIO_PIN_RESET);
#define SSD1306_DESELECT	 HAL_GPIO_WritePin(display.CSPort, display.CSPin, GPIO_PIN_SET);
#define SSD1306_MODE_COMMAND HAL_GPIO_WritePin(display.DCPort, display.DCPin, GPIO_PIN_RESET);
#define SSD1306_MODE_DATA	 HAL_GPIO_WritePin(display.DCPort, display.DCPin, GPIO_PIN_SET);

/* Private function prototypes -----------------------------------------------*/
static void SSD1306_GPIO_Init(void);
static void SSD1306_Reset(void);

static HAL_StatusTypeDef SSD1306_InitSequence(void);

static HAL_StatusTypeDef SSD1306_WriteCommand(uint8_t command);
static HAL_StatusTypeDef SSD1306_WriteCommands(const uint8_t *commands, uint8_t count);
static HAL_StatusTypeDef SSD1306_WriteData(const uint8_t *data, uint16_t count);

static void SSD1306_DrawLine_Bresenham(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool color);

/* Public functions ----------------------------------------------------------*/
HAL_StatusTypeDef SSD1306_Init(SSD1306_InitTypeDef *init)
{
	if (init->SPI == NULL || init->CSPort == NULL || init->DCPort == NULL || init->RSTPort == NULL)
		return HAL_ERROR;

	display = *init;

	SSD1306_Clear(false);

	SSD1306_GPIO_Init();
	SSD1306_Reset();

	HAL_StatusTypeDef status = SSD1306_InitSequence();
	if (status != HAL_OK) return status;

	return SSD1306_Display();
}

HAL_StatusTypeDef SSD1306_Display(void)
{
	static const uint8_t addressWindowCommands[] = {
		SSD1306_COLUMNADDR, 0, SSD1306_WIDTH - 1,
		SSD1306_PAGEADDR, 0, 7
	};

	HAL_StatusTypeDef status = SSD1306_WriteCommands(addressWindowCommands, sizeof(addressWindowCommands));
	if (status != HAL_OK) return status;

	return SSD1306_WriteData(buffer, SSD1306_BUFFER_SIZE);
}

HAL_StatusTypeDef SSD1306_SetContrast(uint8_t contrast)
{
	display.Contrast = contrast;

	HAL_StatusTypeDef status;

	status = SSD1306_WriteCommand(SSD1306_SETCONTRAST);
	if (status != HAL_OK) return status;

	status = SSD1306_WriteCommand(display.Contrast);
	if (status != HAL_OK) return status;

	return status;
}

HAL_StatusTypeDef SSD1306_SetInverted(bool inverted)
{
	display.Inverted = inverted;

	HAL_StatusTypeDef status = SSD1306_WriteCommand(
		display.Inverted ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);

	return status;
}

HAL_StatusTypeDef SSD1306_SetOnOff(bool on)
{
	HAL_StatusTypeDef status = SSD1306_WriteCommand(
		on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);

	return status;
}

void SSD1306_SetPixel(uint8_t x, uint8_t y, bool color)
{
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;

	const uint16_t index = (uint16_t) (x + (y / 8) * SSD1306_WIDTH);
	const uint8_t  mask	 = 1U << (y & 7);

	if (color)
		buffer[index] |= mask;
	else
		buffer[index] &= ~mask;
}

void SSD1306_DrawFastHLine(uint8_t x, uint8_t y, uint8_t width, bool color)
{
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
	if (width == 0) return;

	// Clip right
	if (x + width > SSD1306_WIDTH)
		width = SSD1306_WIDTH - x;

	uint8_t *pBuf = &buffer[(y / 8) * SSD1306_WIDTH + x];
	uint8_t	 mask = 1U << (y & 7);

	if (color)
	{
		while (width--)
			*pBuf++ |= mask;
	}
	else
	{
		while (width--)
			*pBuf++ &= ~mask;
	}
}

void SSD1306_DrawFastVLine(uint8_t x, uint8_t y, uint8_t height, bool color)
{
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
	if (height == 0) return;

	// Clip bottom
	if (y + height > SSD1306_HEIGHT)
		height = SSD1306_HEIGHT - y;

	uint8_t *pBuf = &buffer[(y / 8) * SSD1306_WIDTH + x];
	uint8_t	 mod  = (y & 7);

	// First partial byte
	if (mod)
	{
		mod = 8 - mod;

		static const uint8_t premask[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };

		uint8_t mask = premask[mod];

		// Adjust mask if we don't reach the end of this byte
		if (height < mod)
			mask &= (0XFF >> (mod - height));

		if (color)
			*pBuf |= mask;
		else
			*pBuf &= ~mask;

		// Entire line was drawn in the first byte
		if (height < mod) return;

		pBuf += SSD1306_WIDTH;
		height -= mod;
	}

	// More to go?
	// Write solid bytes - effectively 8 rows at a time
	if (height >= 8)
	{
		uint8_t val = color ? 0xFF : 0x00;

		do
		{
			*pBuf = val;
			pBuf += SSD1306_WIDTH;
			height -= 8;
		}
		while (height >= 8);
	}

	// Final partial byte
	if (height)
	{
		mod = height & 7;

		static const uint8_t postmask[8] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };

		uint8_t mask = postmask[mod];

		if (color)
			*pBuf |= mask;
		else
			*pBuf &= ~mask;
	}
}

void SSD1306_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool color)
{
	if (x0 == x1)
	{
		if (y0 > y1)
			_swap_int8_t(y0, y1);

		SSD1306_DrawFastVLine(x0, y0, y1 - y0 + 1, color);
	}
	else if (y0 == y1)
	{
		if (x0 > x1)
			_swap_int8_t(x0, x1);

		SSD1306_DrawFastHLine(x0, y0, x1 - x0 + 1, color);
	}
	else
		SSD1306_DrawLine_Bresenham(x0, y0, x1, y1, color);
}

void SSD1306_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool color, bool filled)
{
	SSD1306_DrawFastHLine(x, y, width, color);
	SSD1306_DrawFastHLine(x, y + height - 1, width, color);
	SSD1306_DrawFastVLine(x, y, height, color);
	SSD1306_DrawFastVLine(x + width - 1, y, height, color);

	if (filled)
	{
		for (uint8_t i = x; i < x + width; i++)
			SSD1306_DrawFastVLine(i, y, height, color);
	}
}

void SSD1306_DrawTriangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool color, bool filled)
{
	if (!filled)
	{
		SSD1306_DrawLine(x0, y0, x1, y1, color);
		SSD1306_DrawLine(x1, y1, x2, y2, color);
		SSD1306_DrawLine(x2, y2, x0, y0, color);
	}
	else
	{
		uint8_t a, b, y, last;

		// Sort coordinates by Y order (y2 >= y1 >= y0)
		if (y0 > y1)
		{
			_swap_int8_t(y0, y1);
			_swap_int8_t(x0, x1);
		}
		if (y1 > y2)
		{
			_swap_int8_t(y2, y1);
			_swap_int8_t(x2, x1);
		}
		if (y0 > y1)
		{
			_swap_int8_t(y0, y1);
			_swap_int8_t(x0, x1);
		}

		// Handle awkward all-on-same-line case
		if (y0 == y2)
		{
			a = b = x0;

			if (x1 < a)
				a = x1;
			else if (x1 > b)
				b = x1;

			if (x2 < a)
				a = x2;
			else if (x2 > b)
				b = x2;

			SSD1306_DrawFastHLine(a, y0, b - a + 1, color);

			return;
		}

		uint8_t	 dx01 = x1 - x0, dy01 = y1 - y0,
				 dx02 = x2 - x0, dy02 = y2 - y0,
				 dx12 = x2 - x1, dy12 = y2 - y1;
		uint16_t sa = 0, sb = 0;

		if (y1 == y2)
			last = y1; // handle flat-bottomed triangle in first loop
		else
			last = y1 - 1; // skip it and handle in second loop

		// Render the upper part of the triangle (flat-bottom)
		for (y = y0; y <= last; y++)
		{
			a = x0 + sa / dy01;
			b = x0 + sb / dy02;
			sa += dx01;
			sb += dx02;
			// longhand (derived from slope-intercept equation):
			// a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
			// b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
			// And this part (x1 - x0) * (y - y0) we made dx01 * (y - y0)
			// which simply became addition in each loop cycle

			if (a > b)
				_swap_int8_t(a, b);

			SSD1306_DrawFastHLine(a, y, b - a + 1, color);
		}

		// Render the lower part of the triangle (flat-top)
		sa = (uint16_t) dx12 * (y - y1);
		sb = (uint16_t) dx02 * (y - y0);

		for (; y <= y2; y++)
		{
			a = x1 + sa / dy12;
			b = x0 + sb / dy02;
			sa += dx12;
			sb += dx02;
			/* longhand:
			a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
			b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
			*/

			if (a > b)
				_swap_int8_t(a, b);

			SSD1306_DrawFastHLine(a, y, b - a + 1, color);
		}
	}
}

void SSD1306_DrawBitmap(uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t width, uint8_t height, bool color)
{
	if (bitmap == NULL || width == 0 || height == 0) return;

	for (uint8_t sourceY = 0; sourceY < height; ++sourceY)
	{
		const uint8_t page = sourceY / 8;
		const uint8_t bit  = sourceY & 7;

		for (uint8_t sourceX = 0; sourceX < width; ++sourceX)
		{
			uint8_t value = bitmap[page * width + sourceX];
			if (value & (1U << bit))
				SSD1306_SetPixel(x + sourceX, y + sourceY, color);
		}
	}
}

void SSD1306_Clear(bool color)
{
	memset(buffer, color ? 0xFF : 0x00, SSD1306_BUFFER_SIZE);
}

uint8_t *SSD1306_GetBuffer(void) { return buffer; }

/* Private functions ---------------------------------------------------------*/
static void SSD1306_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;

	// CS
	GPIO_InitStruct.Pin	  = display.CSPin;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

	HAL_GPIO_Init(display.CSPort, &GPIO_InitStruct);

	// DC
	GPIO_InitStruct.Pin	  = display.DCPin;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

	HAL_GPIO_Init(display.DCPort, &GPIO_InitStruct);

	// RST
	GPIO_InitStruct.Pin	  = display.RSTPin;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

	HAL_GPIO_Init(display.RSTPort, &GPIO_InitStruct);

	/* Initial pin states */
	SSD1306_DESELECT
	SSD1306_MODE_COMMAND
}

static void SSD1306_Reset(void)
{
	HAL_GPIO_WritePin(display.RSTPort, display.RSTPin, GPIO_PIN_SET);
	HAL_Delay(1);

	HAL_GPIO_WritePin(display.RSTPort, display.RSTPin, GPIO_PIN_RESET);
	HAL_Delay(10);

	HAL_GPIO_WritePin(display.RSTPort, display.RSTPin, GPIO_PIN_SET);
	HAL_Delay(100);
}

static HAL_StatusTypeDef SSD1306_InitSequence(void)
{
	HAL_StatusTypeDef status;

	static const uint8_t init1Commands[] = {
		SSD1306_DISPLAYOFF,
		SSD1306_SETDISPLAYCLOCKDIV, 0x80, // the suggested ratio 0x80
		SSD1306_SETMULTIPLEX, SSD1306_HEIGHT - 1,
		SSD1306_SETDISPLAYOFFSET, 0x00, // no offset
		SSD1306_SETSTARTLINE | 0x00,	// line #0
		SSD1306_CHARGEPUMP, 0x14,		// enable
		SSD1306_MEMORYMODE, 0x00,		// horizontal
		SSD1306_SEGREMAP,
		SSD1306_COMSCAN,
		SSD1306_SETCOMPINS, 0x12,
		SSD1306_SETCONTRAST
	};

	status = SSD1306_WriteCommands(init1Commands, sizeof(init1Commands));
	if (status != HAL_OK) return status;

	status = SSD1306_WriteCommand(display.Contrast);
	if (status != HAL_OK) return status;

	static const uint8_t init2Commands[] = {
		SSD1306_SETPRECHARGE, 0xF1,
		SSD1306_SETVCOMDETECT, 0x40,
		SSD1306_DISPLAYALLON_RESUME
	};

	status = SSD1306_WriteCommands(init2Commands, sizeof(init2Commands));
	if (status != HAL_OK) return status;

	status = SSD1306_WriteCommand(display.Inverted ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
	if (status != HAL_OK) return status;

	static const uint8_t init3Commands[] = {
		SSD1306_DEACTIVATE_SCROLL,
		SSD1306_DISPLAYON
	};

	status = SSD1306_WriteCommands(init3Commands, sizeof(init3Commands));
	if (status != HAL_OK) return status;

	return HAL_OK;
}

static HAL_StatusTypeDef SSD1306_WriteCommand(uint8_t command)
{
	SSD1306_MODE_COMMAND
	SSD1306_SELECT

	HAL_StatusTypeDef status = HAL_SPI_Transmit(display.SPI, &command, 1, display.SPITimeoutMs);

	SSD1306_DESELECT

	return status;
}

static HAL_StatusTypeDef SSD1306_WriteCommands(const uint8_t *commands, uint8_t count)
{
	if (commands == NULL || count == 0)
		return HAL_OK;

	SSD1306_MODE_COMMAND
	SSD1306_SELECT

	HAL_StatusTypeDef status = HAL_SPI_Transmit(display.SPI, (uint8_t *) commands, count, display.SPITimeoutMs);

	SSD1306_DESELECT

	return status;
}

static HAL_StatusTypeDef SSD1306_WriteData(const uint8_t *data, uint16_t count)
{
	if (data == NULL || count == 0)
		return HAL_OK;

	SSD1306_MODE_DATA
	SSD1306_SELECT

	HAL_StatusTypeDef status = HAL_SPI_Transmit(display.SPI, (uint8_t *) data, count, display.SPITimeoutMs);

	SSD1306_DESELECT

	return status;
}

static void SSD1306_DrawLine_Bresenham(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool color)
{
	int16_t dx	  = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
	int16_t sx	  = (x0 < x1) ? 1 : -1;
	int16_t dyAbs = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
	int16_t dy	  = -dyAbs;
	int16_t sy	  = (y0 < y1) ? 1 : -1;
	int16_t error = dx + dy;

	for (;;)
	{
		SSD1306_SetPixel(x0, y0, color);
		if (x0 == x1 && y0 == y1) break;

		const int16_t twiceError = 2 * error;
		if (twiceError >= dy)
		{
			error += dy;
			x0 += sx;
		}
		if (twiceError <= dx)
		{
			error += dx;
			y0 += sy;
		}
	}
}
