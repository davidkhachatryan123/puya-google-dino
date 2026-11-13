/* Includes ------------------------------------------------------------------*/
#include "ssd1306.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static SSD1306_InitTypeDef display;

static uint8_t buffer[SSD1306_BUFFER_SIZE];

/* Private macro -------------------------------------------------------------*/
#define SSD1306_SELECT HAL_GPIO_WritePin(display.CSPort, display.CSPin, GPIO_PIN_RESET);
#define SSD1306_DESELECT HAL_GPIO_WritePin(display.CSPort, display.CSPin, GPIO_PIN_SET);
#define SSD1306_MODE_COMMAND HAL_GPIO_WritePin(display.DCPort, display.DCPin, GPIO_PIN_RESET);
#define SSD1306_MODE_DATA HAL_GPIO_WritePin(display.DCPort, display.DCPin, GPIO_PIN_SET);

/* Private function prototypes -----------------------------------------------*/
static void SSD1306_GPIO_Init(void);
static void SSD1306_Reset(void);

static HAL_StatusTypeDef SSD1306_InitSequence(void);

static HAL_StatusTypeDef SSD1306_WriteCommand(uint8_t command);
static HAL_StatusTypeDef SSD1306_WriteCommands(const uint8_t *commands, uint8_t count);
static HAL_StatusTypeDef SSD1306_WriteData(const uint8_t *data, uint16_t count);

/* Public functions ----------------------------------------------------------*/
HAL_StatusTypeDef SSD1306_Init(SSD1306_InitTypeDef *init)
{
	if (init->SPI == NULL ||
		init->CSPort == NULL ||
		init->DCPort == NULL ||
		init->RSTPort == NULL)
	{
		return HAL_ERROR;
	}

	display = *init;

	SSD1306_Clear(false);

	SSD1306_GPIO_Init();
	SSD1306_Reset();

	HAL_StatusTypeDef status = SSD1306_InitSequence();
	if (status != HAL_OK)
		return status;

	return SSD1306_Display();
}

HAL_StatusTypeDef SSD1306_Display(void)
{
	static const uint8_t addressWindowCommands[] = {
		SSD1306_COLUMNADDR, 0, SSD1306_WIDTH - 1,
		SSD1306_PAGEADDR, 0, 7};

	HAL_StatusTypeDef status = SSD1306_WriteCommands(addressWindowCommands, sizeof(addressWindowCommands));
	if (status != HAL_OK)
		return status;

	return SSD1306_WriteData(buffer, SSD1306_BUFFER_SIZE);
}

HAL_StatusTypeDef SSD1306_SetContrast(uint8_t contrast)
{
	display.Contrast = contrast;

	HAL_StatusTypeDef status;

	status = SSD1306_WriteCommand(SSD1306_SETCONTRAST);
	if (status != HAL_OK)
		return status;

	status = SSD1306_WriteCommand(display.Contrast);
	if (status != HAL_OK)
		return status;

	return status;
}

void SSD1306_Clear(bool color) { memset(buffer, color ? 0xFF : 0x00, SSD1306_BUFFER_SIZE); }

uint8_t *SSD1306_GetBuffer(void) { return buffer; }

/* Private functions ---------------------------------------------------------*/
static void SSD1306_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;

	// CS
	GPIO_InitStruct.Pin = display.CSPin;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

	HAL_GPIO_Init(display.CSPort, &GPIO_InitStruct);

	// DC
	GPIO_InitStruct.Pin = display.DCPin;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

	HAL_GPIO_Init(display.DCPort, &GPIO_InitStruct);

	// RST
	GPIO_InitStruct.Pin = display.RSTPin;
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
		SSD1306_SETCONTRAST};

	status = SSD1306_WriteCommands(init1Commands, sizeof(init1Commands));
	if (status != HAL_OK)
		return status;

	status = SSD1306_WriteCommand(display.Contrast);
	if (status != HAL_OK)
		return status;

	static const uint8_t init2Commands[] = {
		SSD1306_SETPRECHARGE, 0xF1,
		SSD1306_SETVCOMDETECT, 0x40,
		SSD1306_DISPLAYALLON_RESUME};

	status = SSD1306_WriteCommands(init2Commands, sizeof(init2Commands));
	if (status != HAL_OK)
		return status;

	status = SSD1306_WriteCommand(display.Inverted ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
	if (status != HAL_OK)
		return status;

	static const uint8_t init3Commands[] = {
		SSD1306_DEACTIVATE_SCROLL,
		SSD1306_DISPLAYON};

	status = SSD1306_WriteCommands(init3Commands, sizeof(init3Commands));
	if (status != HAL_OK)
		return status;

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

	HAL_StatusTypeDef status = HAL_SPI_Transmit(display.SPI, (uint8_t *)commands, count, display.SPITimeoutMs);

	SSD1306_DESELECT

	return status;
}

static HAL_StatusTypeDef SSD1306_WriteData(const uint8_t *data, uint16_t count)
{
	if (data == NULL || count == 0)
		return HAL_OK;

	SSD1306_MODE_DATA
	SSD1306_SELECT

	HAL_StatusTypeDef status = HAL_SPI_Transmit(display.SPI, (uint8_t *)data, count, display.SPITimeoutMs);

	SSD1306_DESELECT

	return status;
}