/* Includes ------------------------------------------------------------------*/
#include "ssd1306.h"
#include "ssd1306_config.h" // IWYU pragma: keep
#include "ssd1306_char_map.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Private variables ---------------------------------------------------------*/
typedef enum
{
	SSD1306_TRANSPORT_SPI,
	SSD1306_TRANSPORT_I2C
} SSD1306_TransportType;

static SSD1306_TransportType transport;

#if defined(HAL_SPI_MODULE_ENABLED)
static SSD1306_SPI_InitTypeDef *spiDisplay;
#endif
#if defined(HAL_I2C_MODULE_ENABLED)
static SSD1306_I2C_InitTypeDef *i2cDisplay;
#endif

static uint8_t displayWidth;
static uint8_t displayHeight;
static uint8_t displayPages;

static uint8_t *buffer;

static uint8_t _x = 0, _y = 0;
static uint8_t _shift = 0;

static uint8_t _scaleX = 0, _scaleY = 0;

static bool _autoPrintln	  = false;
static bool _skipLeadingSpace = false;

/* Private macro -------------------------------------------------------------*/
#define SSD1306_MAX_X	(displayWidth - 1U)
#define SSD1306_MAX_Y	(displayHeight - 1U)
#define SSD1306_MAX_ROW (displayPages - 1U)

#define SSD1306_ACTIVE_BUFFER_SIZE (displayWidth * displayPages)

#if defined(I2C_BUFFER_LENGTH)
#define SSD1306_I2C_MAX_DATA_SIZE I2C_BUFFER_LENGTH
#else
#define SSD1306_I2C_MAX_DATA_SIZE 32U
#endif

#ifndef _swap_uint8_t
#define _swap_uint8_t(a, b) \
	{                       \
		uint8_t t = a;      \
		a		  = b;      \
		b		  = t;      \
	}
#endif

/* Private function prototypes -----------------------------------------------*/
static void SSD1306_SetDimensions(SSD1306_Type type);

static void SSD1306_SPI_GPIO_Init(void);
static void SSD1306_SPI_Reset(void);

static HAL_StatusTypeDef SSD1306_SPI_Transmit(GPIO_PinState dcState, const uint8_t *data, uint16_t count);
static HAL_StatusTypeDef SSD1306_I2C_Transmit(uint8_t control, const uint8_t *data, uint16_t count);

static HAL_StatusTypeDef SSD1306_WriteCommand(uint8_t command);
static HAL_StatusTypeDef SSD1306_WriteCommands(const uint8_t *commands, uint8_t count);
static HAL_StatusTypeDef SSD1306_WriteData(const uint8_t *data, uint16_t count);

static HAL_StatusTypeDef SSD1306_InitSequence(void);

static HAL_StatusTypeDef SSD1306_SetWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);

static void SSD1306_DrawLine_Bresenham(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool color);
#ifdef SSD1306_TRIANGLE_HORIZONTAL_FILL
static void SSD1306_DrawTriangle_HorizontalFill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool color);
#else
static void SSD1306_DrawTriangle_VerticalFill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool color);
#endif
static void SSD1306_DrawCircleHelper(uint8_t x0, uint8_t y0, uint8_t radius, int8_t cornername, bool color);
static void SSD1306_DrawFilledCircleHelper(uint8_t x0, uint8_t y0, uint8_t radius, uint8_t delta, uint8_t cornername, bool color);

static uint8_t SSD1306_GetFontCharColumn(char character, uint8_t column);
static bool	   SSD1306_WriteByte(uint8_t data, bool color);
static void	   SSD1306_WriteByteOffsetNoShift(uint8_t data, uint8_t verticalOffset, uint8_t horizontalOffset, bool color);
static void	   SSD1306_WriteChar(char character, bool color);

/* Public functions ----------------------------------------------------------*/
#if defined(HAL_SPI_MODULE_ENABLED)
HAL_StatusTypeDef SSD1306_SPI_Init(SSD1306_SPI_InitTypeDef *init)
{
	if (init == NULL || init->SPIHandle == NULL || init->CSPort == NULL || init->DCPort == NULL || init->RSTPort == NULL)
		return HAL_ERROR;

	spiDisplay = init;
	transport  = SSD1306_TRANSPORT_SPI;

	SSD1306_SetDimensions(init->Type);

	if ((!buffer) && !(buffer = (uint8_t *) malloc(SSD1306_ACTIVE_BUFFER_SIZE)))
		return HAL_ERROR;

	SSD1306_SPI_GPIO_Init();
	SSD1306_SPI_Reset();

	HAL_StatusTypeDef status = SSD1306_InitSequence();
	if (status != HAL_OK)
		return status;

	SSD1306_Clear(false);
	SSD1306_Home();

	return SSD1306_Display();
}
#endif

#if defined(HAL_I2C_MODULE_ENABLED)
HAL_StatusTypeDef SSD1306_I2C_Init(SSD1306_I2C_InitTypeDef *init)
{
	if (init == NULL || init->I2CHandle == NULL)
		return HAL_ERROR;

	i2cDisplay = init;
	transport  = SSD1306_TRANSPORT_I2C;

	SSD1306_SetDimensions(init->Type);

	if (init->I2CAddress == NULL)
	{
		i2cDisplay->I2CAddress = init->Type == SSD1306_TYPE_128X64 ? 0x3D : 0x3C;
	}

	HAL_StatusTypeDef status = SSD1306_InitSequence();
	if (status != HAL_OK)
		return status;

	SSD1306_Clear(false);
	SSD1306_Home();

	return SSD1306_Display();
}
#endif

HAL_StatusTypeDef SSD1306_Display(void)
{
	HAL_StatusTypeDef status = SSD1306_SetWindow(0, 0, SSD1306_MAX_X, SSD1306_MAX_ROW);
	if (status != HAL_OK) return status;

	return SSD1306_WriteData(buffer, SSD1306_ACTIVE_BUFFER_SIZE);
}

HAL_StatusTypeDef SSD1306_DisplayPart(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
	if (x0 > x1) _swap_uint8_t(x0, x1);
	if (y0 > y1) _swap_uint8_t(y0, y1);

	if (x0 >= displayWidth || x1 >= displayWidth || y0 >= displayHeight || y1 >= displayHeight)
		return HAL_ERROR;

	y0 >>= 3;
	y1 >>= 3;

	HAL_StatusTypeDef status = SSD1306_SetWindow(x0, y0, x1, y1);
	if (status != HAL_OK) return status;

	for (uint8_t y = y0; y <= y1; y++)
	{
		const uint16_t index = x0 + y * displayWidth;

		status = SSD1306_WriteData(
			&buffer[index],
			x1 - x0 + 1);

		if (status != HAL_OK)
			return status;
	}

	return HAL_OK;
}

HAL_StatusTypeDef SSD1306_SetOnOff(bool on)
{
	HAL_StatusTypeDef status = SSD1306_WriteCommand(
		on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);

	return status;
}

HAL_StatusTypeDef SSD1306_SetContrast(uint8_t contrast)
{
	const uint8_t commands[] = { SSD1306_SETCONTRAST, contrast };

	return SSD1306_WriteCommands(commands, sizeof(commands));
}

HAL_StatusTypeDef SSD1306_SetInverted(bool inverted)
{
	HAL_StatusTypeDef status = SSD1306_WriteCommand(
		inverted ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);

	return status;
}

HAL_StatusTypeDef SSD1306_FlipH(bool mode)
{
	HAL_StatusTypeDef status = SSD1306_WriteCommand(
		mode ? SSD1306_FLIP_H : SSD1306_NORMAL_H);

	return status;
}

HAL_StatusTypeDef SSD1306_FlipV(bool mode)
{
	HAL_StatusTypeDef status = SSD1306_WriteCommand(
		mode ? SSD1306_FLIP_V : SSD1306_NORMAL_V);

	return status;
}

void SSD1306_SetPixel(uint8_t x, uint8_t y, bool color)
{
	if (x >= displayWidth || y >= displayHeight) return;

	const uint16_t index = x + (y >> 3) * displayWidth;
	const uint8_t  mask	 = 1U << (y & 0b111);

	if (color)
		buffer[index] |= mask;
	else
		buffer[index] &= ~mask;
}

void SSD1306_DrawFastHLine(uint8_t x, uint8_t y, uint8_t width, bool color)
{
	if (x >= displayWidth || y >= displayHeight) return;
	if (width == 0) return;

	// Clip right
	if (x + width > displayWidth)
		width = displayWidth - x;

	uint8_t *pBuf = &buffer[(y >> 3) * displayWidth + x];
	uint8_t	 mask = 1U << (y & 0b111);

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
	if (x >= displayWidth || y >= displayHeight) return;
	if (height == 0) return;

	// Clip bottom
	if (y + height > displayHeight)
		height = displayHeight - y;

	uint8_t *pBuf = &buffer[(y >> 3) * displayWidth + x];
	uint8_t	 mod  = (y & 0b111);

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

		pBuf += displayWidth;
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
			pBuf += displayWidth;
			height -= 8;
		}
		while (height >= 8);
	}

	// Final partial byte
	if (height)
	{
		mod = height & 0b111;

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
			_swap_uint8_t(y0, y1);

		SSD1306_DrawFastVLine(x0, y0, y1 - y0 + 1, color);
	}
	else if (y0 == y1)
	{
		if (x0 > x1)
			_swap_uint8_t(x0, x1);

		SSD1306_DrawFastHLine(x0, y0, x1 - x0 + 1, color);
	}
	else
		SSD1306_DrawLine_Bresenham(x0, y0, x1, y1, color);
}

void SSD1306_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool color, bool filled)
{
	if (!filled)
	{
		SSD1306_DrawFastHLine(x, y, width, color);				// Top
		SSD1306_DrawFastHLine(x, y + height - 1, width, color); // Bottom
		SSD1306_DrawFastVLine(x, y, height, color);				// Left
		SSD1306_DrawFastVLine(x + width - 1, y, height, color); // Right
	}
	else
	{
		for (uint8_t i = x; i < x + width; i++)
			SSD1306_DrawFastVLine(i, y, height, color);
	}
}

void SSD1306_DrawRoundRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t radius, bool color, bool filled)
{
	int16_t max_radius = ((width < height) ? width : height) / 2;
	if (radius > max_radius)
		radius = max_radius;

	if (!filled)
	{
		SSD1306_DrawFastHLine(x + radius, y, width - 2 * radius, color);			  // Top
		SSD1306_DrawFastHLine(x + radius, y + height - 1, width - 2 * radius, color); // Bottom
		SSD1306_DrawFastVLine(x, y + radius, height - 2 * radius, color);			  // Left
		SSD1306_DrawFastVLine(x + width - 1, y + radius, height - 2 * radius, color); // Right

		SSD1306_DrawCircleHelper(x + radius, y + radius, radius, 0x1, color);						   // Top-Left
		SSD1306_DrawCircleHelper(x + width - radius - 1, y + radius, radius, 0x2, color);			   // Top-Right
		SSD1306_DrawCircleHelper(x + width - radius - 1, y + height - radius - 1, radius, 0x4, color); // Bottom-Right
		SSD1306_DrawCircleHelper(x + radius, y + height - radius - 1, radius, 0x8, color);			   // Bottom-Left
	}
	else
	{
		SSD1306_DrawRectangle(x + radius, y + radius, width - 2 * radius, height, color, filled);

		SSD1306_DrawFilledCircleHelper(x + width - radius - 1, y + radius, radius, height - 2 * radius - 1, 0x1, color);
		SSD1306_DrawFilledCircleHelper(x + radius, y + radius, radius, height - 2 * radius - 1, 0x2, color);
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
#ifdef SSD1306_TRIANGLE_HORIZONTAL_FILL
		SSD1306_DrawTriangle_HorizontalFill(x0, y0, x1, y1, x2, y2, color);
#else
		SSD1306_DrawTriangle_VerticalFill(x0, y0, x1, y1, x2, y2, color);
#endif
	}
}

void SSD1306_DrawCircle(uint8_t x0, uint8_t y0, uint8_t radius, bool color, bool filled)
{
	if (!filled)
		SSD1306_DrawCircleHelper(x0, y0, radius, 0x1 | 0x2 | 0x4 | 0x8, color);
	else
		SSD1306_DrawFilledCircleHelper(x0, y0, radius, 0, 0x1 | 0x2, color);
}

void SSD1306_DrawBitmap(uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t width, uint8_t height, bool color)
{
	if (bitmap == NULL || width == 0 || height == 0) return;

	for (uint8_t sourceY = 0; sourceY < height; ++sourceY)
	{
		const uint8_t page = sourceY >> 3;
		const uint8_t bit  = sourceY & 0b111;

		for (uint8_t sourceX = 0; sourceX < width; ++sourceX)
		{
			uint8_t value = bitmap[page * width + sourceX];
			if (value & (1U << bit))
				SSD1306_SetPixel(x + sourceX, y + sourceY, color);
		}
	}
}

void SSD1306_Home(void)
{
	_x	   = 0;
	_y	   = 0;
	_shift = 0;
}

void SSD1306_SetCursor(uint8_t x, uint8_t y)
{
	if (x > SSD1306_MAX_X || y > SSD1306_MAX_Y) return;

	_x	   = x;
	_y	   = y;
	_shift = _y & 0b111;
}

void SSD1306_SetScale(uint8_t scale)
{
	if (scale < 1) scale = 1;
	else if (scale > 4) scale = 4;

	_scaleX = scale;
	_scaleY = scale * 8;
}

void SSD1306_SetAutoPrintln(bool enabled) { _autoPrintln = enabled; }

void SSD1306_DrawByte(uint8_t data, bool color)
{
	bool written = SSD1306_WriteByte(data, color);

	if (written)
		_x++;
}

void SSD1306_Print(const char *text, bool color)
{
	while (*text)
		SSD1306_WriteChar(*text++, color);
}

void SSD1306_Println(const char *text, bool color)
{
	SSD1306_Print(text, color);
	SSD1306_Print("\r\n", color);
}

void SSD1306_Clear(bool color)
{
	memset(buffer, color ? 0xFF : 0x00, SSD1306_ACTIVE_BUFFER_SIZE);
}

void SSD1306_ClearPart(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool color)
{
	if (x0 > x1) _swap_uint8_t(x0, x1);
	if (y0 > y1) _swap_uint8_t(y0, y1);

	SSD1306_DrawRectangle(x0, y0, x1 - x0 + 1, y1 - y0 + 1, color, true);
}

uint8_t *SSD1306_GetBuffer(void) { return buffer; }

/* Private functions ---------------------------------------------------------*/
static void SSD1306_SetDimensions(SSD1306_Type type)
{
	switch (type)
	{
	case SSD1306_TYPE_128X64:
		displayWidth  = 128U;
		displayHeight = 64U;
		break;

	case SSD1306_TYPE_128X32:
		displayWidth  = 128U;
		displayHeight = 32U;
		break;

	default:
		displayWidth  = 0U;
		displayHeight = 0U;
		break;
	}

	displayPages = displayHeight / 8;
}

static void SSD1306_SPI_GPIO_Init(void)
{
	/* Initial pin states */
	HAL_GPIO_WritePin(
		spiDisplay->CSPort,
		spiDisplay->CSPin,
		GPIO_PIN_SET);

	HAL_GPIO_WritePin(
		spiDisplay->DCPort,
		spiDisplay->DCPin,
		GPIO_PIN_RESET);

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;

	// CS
	GPIO_InitStruct.Pin	  = spiDisplay->CSPin;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

	HAL_GPIO_Init(spiDisplay->CSPort, &GPIO_InitStruct);

	// DC
	GPIO_InitStruct.Pin	  = spiDisplay->DCPin;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

	HAL_GPIO_Init(spiDisplay->DCPort, &GPIO_InitStruct);

	// RST
	GPIO_InitStruct.Pin	  = spiDisplay->RSTPin;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

	HAL_GPIO_Init(spiDisplay->RSTPort, &GPIO_InitStruct);
}

static void SSD1306_SPI_Reset(void)
{
	HAL_GPIO_WritePin(spiDisplay->RSTPort, spiDisplay->RSTPin, GPIO_PIN_SET);
	HAL_Delay(1);

	HAL_GPIO_WritePin(spiDisplay->RSTPort, spiDisplay->RSTPin, GPIO_PIN_RESET);
	HAL_Delay(20);

	HAL_GPIO_WritePin(spiDisplay->RSTPort, spiDisplay->RSTPin, GPIO_PIN_SET);
	HAL_Delay(100);
}

static HAL_StatusTypeDef SSD1306_SPI_Transmit(GPIO_PinState dcState, const uint8_t *data, uint16_t count)
{
#if defined(HAL_SPI_MODULE_ENABLED)
	HAL_GPIO_WritePin(
		spiDisplay->DCPort,
		spiDisplay->DCPin,
		dcState);

	HAL_GPIO_WritePin(
		spiDisplay->CSPort,
		spiDisplay->CSPin,
		GPIO_PIN_RESET);

	HAL_StatusTypeDef status = HAL_SPI_Transmit(
		spiDisplay->SPIHandle,
		(uint8_t *) data,
		count,
		SSD1306_SPI_TIMEOUT);

	while (__HAL_SPI_GET_FLAG(spiDisplay->SPIHandle, SPI_FLAG_BSY)) {}

	HAL_GPIO_WritePin(
		spiDisplay->CSPort,
		spiDisplay->CSPin,
		GPIO_PIN_SET);

	return status;
#else
	return HAL_ERROR;
#endif
}

static HAL_StatusTypeDef SSD1306_I2C_Transmit(uint8_t control, const uint8_t *data, uint16_t count)
{
#if defined(HAL_I2C_MODULE_ENABLED)
	if (data == NULL || count == 0)
		return HAL_OK;

	uint8_t packet[SSD1306_I2C_MAX_DATA_SIZE + 1];

	while (count > 0)
	{
		uint16_t chunk = count;

		if (chunk > SSD1306_I2C_MAX_DATA_SIZE)
			chunk = SSD1306_I2C_MAX_DATA_SIZE;

		packet[0] = control;

		memcpy(&packet[1], data, chunk);

		HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
			i2cDisplay->I2CHandle,
			(uint16_t) (i2cDisplay->I2CAddress << 1),
			packet,
			chunk + 1,
			SSD1306_I2C_TIMEOUT);

		while (HAL_I2C_GetState(i2cDisplay->I2CHandle) != HAL_I2C_STATE_READY) {}

		if (status != HAL_OK)
			return status;

		data += chunk;
		count -= chunk;
	}

	return HAL_OK;
#else
	return HAL_ERROR;
#endif
}

static inline HAL_StatusTypeDef SSD1306_WriteCommand(uint8_t command)
{
	return SSD1306_WriteCommands(&command, 1);
}

static HAL_StatusTypeDef SSD1306_WriteCommands(const uint8_t *commands, uint8_t count)
{
	if (commands == NULL || count == 0)
		return HAL_OK;

	if (transport == SSD1306_TRANSPORT_SPI)
	{
		return SSD1306_SPI_Transmit(
			GPIO_PIN_RESET,
			commands,
			count);
	}

	return SSD1306_I2C_Transmit(
		SSD1306_I2C_CONTROL_COMMAND,
		commands,
		count);
}

static HAL_StatusTypeDef SSD1306_WriteData(const uint8_t *data, uint16_t count)
{
	if (data == NULL || count == 0)
		return HAL_OK;

	if (transport == SSD1306_TRANSPORT_SPI)
	{
		return SSD1306_SPI_Transmit(
			GPIO_PIN_SET,
			data,
			count);
	}

	return SSD1306_I2C_Transmit(
		SSD1306_I2C_CONTROL_DATA,
		data,
		count);
}

static HAL_StatusTypeDef SSD1306_InitSequence(void)
{
	uint8_t compins	 = (displayHeight == 64) ? 0x12 : 0x02;
	uint8_t contrast = (displayHeight == 64) ? 0x9F : 0x8F;

	const uint8_t initCommands[] = {
		SSD1306_DISPLAYOFF,

		SSD1306_SETDISPLAYCLOCKDIV, 0x80, // the suggested ratio 0x80

		SSD1306_SETMULTIPLEX, SSD1306_MAX_Y,

		SSD1306_SETDISPLAYOFFSET, 0x00, // no offset
		SSD1306_SETSTARTLINE | 0x00,	// line #0

		SSD1306_CHARGEPUMP, 0x14, // enable

		SSD1306_MEMORYMODE, 0x00, // horizontal

		SSD1306_NORMAL_H,
		SSD1306_NORMAL_V,

		SSD1306_SETCOMPINS, compins,

		SSD1306_SETCONTRAST, contrast,

		SSD1306_SETPRECHARGE, 0xF1,
		SSD1306_SETVCOMDETECT, 0x40,

		SSD1306_DISPLAYALLON_RESUME,
		SSD1306_NORMALDISPLAY,
		SSD1306_DEACTIVATE_SCROLL,
		SSD1306_DISPLAYON
	};

	HAL_StatusTypeDef status = SSD1306_WriteCommands(initCommands, sizeof(initCommands));
	return status;
}

static HAL_StatusTypeDef SSD1306_SetWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
	if (x0 > SSD1306_MAX_X) x0 = SSD1306_MAX_X;
	if (x1 > SSD1306_MAX_X) x1 = SSD1306_MAX_X;

	if (y0 > SSD1306_MAX_ROW) y0 = SSD1306_MAX_ROW;
	if (y1 > SSD1306_MAX_ROW) y1 = SSD1306_MAX_ROW;

	uint8_t addressWindowCommands[] = {
		SSD1306_COLUMNADDR, x0, x1,
		SSD1306_PAGEADDR, y0, y1
	};

	HAL_StatusTypeDef status = SSD1306_WriteCommands(addressWindowCommands, sizeof(addressWindowCommands));
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

#ifdef SSD1306_TRIANGLE_HORIZONTAL_FILL
static void SSD1306_DrawTriangle_HorizontalFill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool color)
{
	int16_t a, b;
	int16_t y, last;

	// Sort coordinates by Y order (y2 >= y1 >= y0)
	if (y0 > y1)
	{
		_swap_uint8_t(y0, y1);
		_swap_uint8_t(x0, x1);
	}
	if (y1 > y2)
	{
		_swap_uint8_t(y2, y1);
		_swap_uint8_t(x2, x1);
	}
	if (y0 > y1)
	{
		_swap_uint8_t(y0, y1);
		_swap_uint8_t(x0, x1);
	}

	// Handle awkward all-on-same-line case
	if (y0 == y2)
	{
		a = b = x0;

		if (x1 < a) a = x1;
		else if (x1 > b) b = x1;

		if (x2 < a) a = x2;
		else if (x2 > b) b = x2;

		SSD1306_DrawFastHLine(a, y0, b - a + 1, color);
		return;
	}

	int16_t dx01 = x1 - x0, dy01 = y1 - y0,
			dx02 = x2 - x0, dy02 = y2 - y0,
			dx12 = x2 - x1, dy12 = y2 - y1;
	int32_t sa = 0, sb = 0;

	if (y1 == y2)
		last = y1; // handle flat-bottomed triangle in first loop
	else
		last = y1 - 1; // skip it and handle in second loop

	// Render the upper part of the triangle (flat-bottom)
	for (y = y0; y <= last; y++)
	{
		a = (int16_t) x0 + sa / dy01;
		b = (int16_t) x0 + sb / dy02;
		sa += dx01;
		sb += dx02;
		// longhand (derived from slope-intercept equation):
		// a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
		// b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
		// And this part (x1 - x0) * (y - y0) we made dx01 * (y - y0)
		// which simply became addition in each loop cycle

		if (a > b)
			_swap_uint8_t(a, b);

		SSD1306_DrawFastHLine(a, y, b - a + 1, color);
	}

	// Render the lower part of the triangle (flat-top)
	sa = (int32_t) dx12 * (y - y1);
	sb = (int32_t) dx02 * (y - y0);

	for (; y <= y2; y++)
	{
		a = (int16_t) x1 + sa / dy12;
		b = (int16_t) x0 + sb / dy02;
		sa += dx12;
		sb += dx02;
		/* longhand:
		a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
		b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
		*/

		if (a > b)
			_swap_uint8_t(a, b);

		SSD1306_DrawFastHLine(a, y, b - a + 1, color);
	}
}
#else
static void SSD1306_DrawTriangle_VerticalFill(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool color)
{
	int16_t a, b;
	int16_t x, last;

	// Sort coordinates by X order (x2 >= x1 >= x0)
	if (x0 > x1)
	{
		_swap_uint8_t(x0, x1);
		_swap_uint8_t(y0, y1);
	}
	if (x1 > x2)
	{
		_swap_uint8_t(x2, x1);
		_swap_uint8_t(y2, y1);
	}
	if (x0 > x1)
	{
		_swap_uint8_t(x0, x1);
		_swap_uint8_t(y0, y1);
	}

	// Handle awkward all-on-same-line case
	if (x0 == x2)
	{
		a = b = y0;

		if (y1 < a) a = y1;
		else if (y1 > b) b = y1;

		if (y2 < a) a = y2;
		else if (y2 > b) b = y2;

		SSD1306_DrawFastVLine(x0, a, b - a + 1, color);
		return;
	}

	int16_t dx01 = x1 - x0, dy01 = y1 - y0,
			dx02 = x2 - x0, dy02 = y2 - y0,
			dx12 = x2 - x1, dy12 = y2 - y1;
	int32_t sa = 0, sb = 0;

	if (x1 == x2)
		last = x1; // handle flat-righted triangle in first loop
	else
		last = x1 - 1; // skip it and handle in second loop

	// Render the left part of the triangle (flat-right)
	for (x = x0; x <= last; x++)
	{
		a = (int16_t) y0 + sa / dx01;
		b = (int16_t) y0 + sb / dx02;
		sa += dy01;
		sb += dy02;
		// longhand (derived from slope-intercept equation):
		// a = y0 + (y1 - y0) * (x - x0) / (x1 - x0);
		// b = y0 + (y2 - y0) * (x - x0) / (x2 - x0);
		// And this part (y1 - y0) * (x - x0) we made dy01 * (x - x0)
		// which simply became addition in each loop cycle

		if (a > b)
			_swap_uint8_t(a, b);

		SSD1306_DrawFastVLine(x, a, b - a + 1, color);
	}

	// Render the right part of the triangle (flat-left)
	sa = (int32_t) dy12 * (x - x1);
	sb = (int32_t) dy02 * (x - x0);

	for (; x <= x2; x++)
	{
		a = (int16_t) y1 + sa / dx12;
		b = (int16_t) y0 + sb / dx02;
		sa += dy12;
		sb += dy02;
		/* longhand:
		a = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
		b = y0 + (y2 - y0) * (x - x0) / (x2 - x0);
		*/

		if (a > b)
			_swap_uint8_t(a, b);

		SSD1306_DrawFastVLine(x, a, b - a + 1, color);
	}
}
#endif

static void SSD1306_DrawCircleHelper(uint8_t x0, uint8_t y0, uint8_t radius, int8_t cornername, bool color)
{
	int16_t x = 0, y = radius;
	int16_t p = 3 - 2 * radius;

	int16_t cx = x0, cy = y0;

	while (x <= y)
	{
		if (cornername & 0x4)
			SSD1306_SetPixel(cx + x, cy + y, color);

		if (cornername & 0x8)
			if (x != 0)
				SSD1306_SetPixel(cx - x, cy + y, color);

		if (cornername & 0x2)
			if (x != 0)
				SSD1306_SetPixel(cx + x, cy - y, color);

		if (cornername & 0x1)
			SSD1306_SetPixel(cx - x, cy - y, color);

		if (x != y)
		{
			if (cornername & 0x4)
				SSD1306_SetPixel(cx + y, cy + x, color);
			if (cornername & 0x8)
				SSD1306_SetPixel(cx - y, cy + x, color);
			if (cornername & 0x2)
				SSD1306_SetPixel(cx + y, cy - x, color);
			if (cornername & 0x1)
				SSD1306_SetPixel(cx - y, cy - x, color);
		}

		if (p < 0)
		{
			p += 4 * x + 6;
		}
		else
		{
			y--;
			p += 4 * (x - y) + 10;
		}

		x++;
	}
}

static void SSD1306_DrawFilledCircleHelper(uint8_t x0, uint8_t y0, uint8_t radius, uint8_t delta, uint8_t cornername, bool color)
{
	int16_t x = 0, y = radius;
	int16_t p = 3 - 2 * radius;

	int16_t cx = x0, cy = y0;

	delta++; // escaping "+1"s in drawing functions

	while (x <= y)
	{
		if (cornername & 0x1)
			SSD1306_DrawFastVLine(cx + x, cy - y, 2 * y + delta, color);

		if (cornername & 0x2)
			if (x != 0)
				SSD1306_DrawFastVLine(cx - x, cy - y, 2 * y + delta, color);

		if (x != y)
		{
			if (cornername & 0x1)
				SSD1306_DrawFastVLine(cx + y, cy - x, 2 * x + delta, color);
			if (cornername & 0x2)
				SSD1306_DrawFastVLine(cx - y, cy - x, 2 * x + delta, color);
		}

		if (p < 0)
		{
			p += 4 * x + 6;
		}
		else
		{
			y--;
			p += 4 * (x - y) + 10;
		}

		x++;
	}
}

static uint8_t SSD1306_GetFontCharColumn(char character, uint8_t column)
{
	if (character < SSD1306_FONT_FIRST_CHAR || SSD1306_FONT_LAST_CHAR > 126 || column >= SSD1306_FONT_WIDTH)
		return 0;

	const uint8_t index = (uint8_t) character - 32;

	return SSD1306_CHAR_MAP[index][column];
}

static bool SSD1306_WriteByte(uint8_t data, bool color)
{
	if (_x > SSD1306_MAX_X) return false;

	const uint16_t index = _x + (_y >> 3) * displayWidth;

	if (_shift == 0)
	{
		if (color)
			buffer[index] |= data;
		else
			buffer[index] &= ~data;
	}
	else
	{
		const uint8_t lower = data << _shift;
		const uint8_t upper = data >> (8 - _shift);

		if (color)
		{
			buffer[index] |= lower;
			if (_y >> 3 < SSD1306_MAX_ROW)
				buffer[index + displayWidth] |= upper;
		}
		else
		{
			buffer[index] &= ~lower;
			if (_y >> 3 < SSD1306_MAX_ROW)
				buffer[index + displayWidth] &= ~upper;
		}
	}

	return true;
}

static void SSD1306_WriteByteOffsetNoShift(uint8_t data, uint8_t verticalOffset, uint8_t horizontalOffset, bool color)
{
	const uint16_t index = (_x + horizontalOffset) + ((_y >> 3) + verticalOffset) * displayWidth;

	if (color)
		buffer[index] |= data;
	else
		buffer[index] &= ~data;
}

static void SSD1306_WriteChar(char character, bool color)
{
	bool newPos = false;

	// Handle carriage return.
	if (character == '\r')
	{
		_x		  = 0;
		character = 0;
		newPos	  = true;
	}

	// Handle line feed.
	if (character == '\n')
	{
		_y += _scaleY;
		character		  = 0;
		newPos			  = true;
		_skipLeadingSpace = true;
	}

	// Handle automatic line wrapping.
	if (_autoPrintln && (_x + _scaleX * (SSD1306_FONT_WIDTH + 1) - 1) > SSD1306_MAX_X)
	{
		_x = 0;
		_y += _scaleY;
		newPos = true;
	}

	// Update cursor position if necessary.
	if (newPos)
		SSD1306_SetCursor(_x, _y);

	// Stop if the display boundary is exceeded.
	if (_y + _scaleY - 1 > SSD1306_MAX_Y)
		character = 0;

	// Skip the first space after a line break.
	if (_skipLeadingSpace && _autoPrintln && character == ' ' && _x == 0)
	{
		_skipLeadingSpace = 0;
		character		  = 0;
	}

	// Nothing to print.
	if (character == 0) return;

	int newX = _x + _scaleX * (SSD1306_FONT_WIDTH + 1);
	if (newX < 0 || _x > SSD1306_MAX_X) _x = newX; // Skip writing outside of display.
	else
	{
		for (uint8_t col = 0; col <= SSD1306_FONT_WIDTH; col++)
		{
			uint8_t bits = SSD1306_GetFontCharColumn(character, col);
			if (!color) bits = ~bits;

			if (_scaleX == 1)
				SSD1306_WriteByte(bits, color);
			else // Shrink font for scales: 2, 3 or 4
			{
				// Scaling bits in direction of Y.
				uint32_t newData = 0;
				for (uint8_t sourceBit = 0, targetBit = 0; sourceBit < 8; sourceBit++)
				{
					for (uint8_t scaleOffset = 0; scaleOffset < _scaleX; scaleOffset++, targetBit++)
					{
						newData |= (uint32_t) ((bits >> sourceBit) & 1U) << targetBit; // Copies the source bit to its corresponding position in the target.
					}
				}

				for (uint8_t horizontalScaleOffset = 0; horizontalScaleOffset < _scaleX; horizontalScaleOffset++)
				{
					uint8_t previousByte = 0;

					if (_x + horizontalScaleOffset >= 0 && _x + horizontalScaleOffset <= SSD1306_MAX_X)
					{
						for (uint8_t verticalScaleOffset = 0; verticalScaleOffset < _scaleX; verticalScaleOffset++)
						{
							// Extract the next 8-bit segment from the scaled data.
							uint8_t currentByte = newData >> (verticalScaleOffset * 8);

							if (_shift == 0)
							{
								// Write the bitmap segment directly without vertical shifting.
								SSD1306_WriteByteOffsetNoShift(currentByte, verticalScaleOffset, horizontalScaleOffset, color);
							}
							else
							{
								// Merge the previous segment's overflow with the current segment.
								SSD1306_WriteByteOffsetNoShift(
									(previousByte >> (8 - _shift)) | (currentByte << _shift),
									verticalScaleOffset, horizontalScaleOffset, color);

								// Preserve the current segment for the next iteration.
								previousByte = currentByte;
							}
						}
					}

					// Write the remaining overflow bits into the next vertical segment.
					if (_shift != 0)
					{
						SSD1306_WriteByteOffsetNoShift(
							previousByte >> (8 - _shift),
							_scaleX, horizontalScaleOffset, color);
					}
				}
			}

			_x += _scaleX;
		}
	}
}
