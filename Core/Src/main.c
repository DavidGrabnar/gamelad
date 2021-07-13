/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdarg.h>
#include<time.h>
#include <math.h>
#include "ssd1351.h"
#include "fonts.h"
#include "testimg.h"
#include "snake_icon.h"
#include "pong_icon.h"
#include "queue.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

I2C_HandleTypeDef hi2c1;

I2S_HandleTypeDef hi2s3;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */

#define BUFSIZE 256
char send_buffer[BUFSIZE];
int counter;
int key_state = 0;

uint32_t prev = 0;
uint32_t current = 0;

// joystick settings
#define JS_MIN 0
#define JS_MAX 1023
#define JS_ZERO ((JS_MAX - JS_MIN) / 2)
#define JS_TOLERANCE 20

// view breakdown
#define HEADER_BOTTOM 16
#define FOOTER_TOP (127 - 16)

enum View{
	MENU,
	SNAKE,
	PONG
} curr_view;

// menu
#define ICON_WIDTH 32
#define ICON_HEIGHT 32
#define ICON_PADDING 8

int selected_icon_index = 0;

// snake
#define SN_UNIT 4
#define SN_ROOM_SIZE (128 / SN_UNIT)
#define SN_MIN_X 1
#define SN_MAX_X (SN_ROOM_SIZE - 2)
#define SN_MIN_Y ((HEADER_BOTTOM / SN_UNIT) + 1)
#define SN_MAX_Y ((FOOTER_TOP / SN_UNIT) - 1)
struct Queue* snake_body;

int snake_x = 0;
int snake_y = 0;
int direction = 1; // [0-3]

int fruit_x = 0;
int fruit_y = 0;

// pong
#define PG_UNIT 4
#define PG_ROOM_SIZE (128 / PG_UNIT)
#define PG_PADDLE_OFFSET 1
#define PG_PADDLE_LENGTH 5
#define PG_MIN_X 1
#define PG_MAX_X (PG_ROOM_SIZE - 2)
#define PG_MIN_Y ((HEADER_BOTTOM / PG_UNIT) + 1)
#define PG_MAX_Y ((FOOTER_TOP / PG_UNIT) - 1)

#define PG_SPEED 1

struct Ball {
	int x, y;
	float direction;
};
struct Ball* pg_ball;
int pg_paddle_player = 0;
int pg_paddle_bot = 0;
int pg_score = 0;

// TODO refactor joystick handler


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2S3_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
/* USER CODE BEGIN PFP */
static uint32_t readAnalog(void);
static uint32_t readAnalog2(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void init() {
    logSerial("Welcome to Gamelad v0.1!\r\n");
    SSD1351_Unselect();
    SSD1351_Init();
    SSD1351_FillScreen(SSD1351_BLACK);

    HAL_Delay(100);

    curr_view = MENU;

    initStartup();

    HAL_Delay(100);

    initHeader();

    HAL_Delay(100);

    logSerial("Init started!\r\n");

    switch(curr_view) {
    case MENU:
    	initMenu();
        logSerial("Init menu done\r\n");
    	break;
    case SNAKE:
    	initSnake();
        logSerial("Init snake done\r\n");
    	break;
    case PONG:
    	initPong();
        logSerial("Init pong done\r\n");
    	break;
    	snprintf(send_buffer, BUFSIZE, "Invalid view on init: %d\r\n", curr_view);
    	logSerial(send_buffer);
    }

    HAL_Delay(100);

    logSerial("Init done! :)\r\n");
}

void initStartup() { // TODO make pretty
    logSerial("Startup started!\r\n");
	SSD1351_WriteString(0, 0, "Gamelad v0.1", Font_16x26, SSD1351_COLOR565(255, 127, 0), SSD1351_BLACK);

    HAL_Delay(3000);

    SSD1351_FillScreen(SSD1351_BLACK);

    strokeRect(0, HEADER_BOTTOM, 127, FOOTER_TOP - HEADER_BOTTOM, SSD1351_WHITE);

    logSerial("Startup done!\r\n");
}

void initHeader() {
    SSD1351_WriteString(0, 0, "Gamelad v0.1", Font_7x10, SSD1351_COLOR565(255, 127, 0), SSD1351_BLACK);
}

void initMenu() { // TODO implement to allow break into two / three rows
	SSD1351_FillRectangle(1, HEADER_BOTTOM + 1, 127 - 2, FOOTER_TOP - HEADER_BOTTOM - 2, SSD1351_BLACK);

	SSD1351_FillRectangle(0, FOOTER_TOP, 127, 127, SSD1351_BLACK);

	SSD1351_DrawImage(ICON_PADDING, HEADER_BOTTOM + ICON_PADDING, 32, 32, (const uint16_t*)snake_icon);
	SSD1351_DrawImage(2 * ICON_PADDING + ICON_WIDTH, HEADER_BOTTOM + ICON_PADDING, 32, 32, (const uint16_t*)pong_icon);

	for(int i = 0; i < 6; i++) {
		int x = ((i % 3) + 1) * ICON_PADDING + (i % 3) * ICON_WIDTH;
		int y = ((i / 3) + 1 + (i / 3 > 0 ? 1 : 0)) * ICON_PADDING + (i / 3) * ICON_HEIGHT + HEADER_BOTTOM;
		strokeRect(x, y, ICON_WIDTH, ICON_HEIGHT, SSD1351_RED);
		if (selected_icon_index == i) {
			SSD1351_FillRectangle(x - 1, y - 1, ICON_WIDTH + 2, 2, SSD1351_BLUE);
			SSD1351_FillRectangle(x - 1, y + ICON_HEIGHT - 1, ICON_WIDTH + 2, 2, SSD1351_BLUE);

			SSD1351_FillRectangle(x - 1, y + 1, 2, ICON_HEIGHT, SSD1351_BLUE);
			SSD1351_FillRectangle(x + ICON_WIDTH - 1, y + 1, 2, ICON_HEIGHT, SSD1351_BLUE);
		}
	}


	char *name;
	switch(selected_icon_index) {
	case 0:
		name = "Snake";
		break;
	case 1:
		name = "Pong";
		break;
	default:
		name = "_";
	}

	snprintf(send_buffer, BUFSIZE, "Select game: %s", name);
    SSD1351_WriteString(0, FOOTER_TOP + 4, send_buffer, Font_7x10, SSD1351_WHITE, SSD1351_BLACK);
}

void initSnake() {
	SSD1351_FillRectangle(1, HEADER_BOTTOM + 1, 127 - 2, FOOTER_TOP - HEADER_BOTTOM - 2, SSD1351_BLACK);

	SSD1351_FillRectangle(0, FOOTER_TOP, 127, 127, SSD1351_BLACK);

	snake_x = 10;
	snake_y = 10;

	snake_body = createQueue(SN_ROOM_SIZE * SN_ROOM_SIZE);
	enqueue(snake_body, snake_x * SN_ROOM_SIZE + snake_y);

	renderBox(snake_x, snake_y, SSD1351_WHITE);

	fruit_x = rand() % SN_ROOM_SIZE;
	fruit_y = rand() % SN_ROOM_SIZE;

	renderBox(fruit_x, fruit_y, SSD1351_RED);

	snprintf(send_buffer, BUFSIZE, "Score: %d", size(snake_body));
    SSD1351_WriteString(0, FOOTER_TOP + 4, send_buffer, Font_7x10, SSD1351_WHITE, SSD1351_BLACK);
}

void initPong() {
	SSD1351_FillRectangle(1, HEADER_BOTTOM + 1, 127 - 2, FOOTER_TOP - HEADER_BOTTOM - 2, SSD1351_BLACK);

	SSD1351_FillRectangle(0, FOOTER_TOP, 127, 127, SSD1351_BLACK);

	pg_ball = (struct Ball*) malloc(sizeof(struct Ball));

	pg_ball->x = 10 * PG_UNIT;
	pg_ball->y = 10 * PG_UNIT;
	pg_ball->direction = 2 * M_PI / 3;

	pg_score = 0;

    SSD1351_WriteString(0, FOOTER_TOP + 4, "Score:", Font_7x10, SSD1351_WHITE, SSD1351_BLACK);
}

void strokeRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
	for(int cx = x; cx < x + w; cx++) {
        SSD1351_DrawPixel(cx, y, color);
        SSD1351_DrawPixel(cx, y + h - 1, color);
    }

    for(int cy = y; cy < y + h; cy++) {
        SSD1351_DrawPixel(x, cy, color);
        SSD1351_DrawPixel(x + w - 1, cy, color);
    }
}

void logSerial(const char* message) {
	snprintf(send_buffer, BUFSIZE, message);
	CDC_Transmit_FS(send_buffer, strlen(send_buffer));
}


void loop() {
  switch(curr_view) {
  case MENU:
	menuLoop();
	break;
  case SNAKE:
	snakeLoop();
	break;
  case PONG:
	pongLoop();
	break;
	// TODO default handler - log error
  }
}

void snakeLoop() {
	uint32_t analogValue = readAnalog();
	uint32_t analogValue2 = readAnalog2();

	if (analogValue < JS_ZERO - JS_TOLERANCE && direction != 1)
	{
	  direction = 3;
	} else if (analogValue > JS_ZERO + JS_TOLERANCE && direction != 3)
	{
	  direction = 1;
	} else if (analogValue2 > JS_ZERO + JS_TOLERANCE && direction != 2)
	{
	  direction = 0;
	} else if (analogValue2 < JS_ZERO - JS_TOLERANCE && direction != 0)
	{
	  direction = 2;
	}

	if (direction == 0) {
	  snake_y -= 1;
	  if (snake_y < SN_MIN_Y) {
		  snake_y = SN_MAX_Y;
	  }
	} else if (direction == 1) {
	  snake_x += 1;
	  if (snake_x > SN_MAX_X) {
		  snake_x = SN_MIN_X;
	  }
	} else if (direction == 2) {
	  snake_y += 1;
	  if (snake_y > SN_MAX_Y) {
		  snake_y = SN_MIN_Y;
	  }
	} else if (direction == 3) {
	  snake_x -= 1;
	  if (snake_x < SN_MIN_X) {
		  snake_x = SN_MAX_X;
	  }
	}

	enqueue(snake_body, snake_x * SN_ROOM_SIZE + snake_y);

	renderBox(snake_x, snake_y, SSD1351_WHITE);

	if (snake_x == fruit_x && snake_y == fruit_y) {
	  fruit_x = rand() % (SN_MAX_X - SN_MIN_X) + SN_MIN_X;
	  fruit_y = rand() % (SN_MAX_Y - SN_MIN_Y) + SN_MIN_Y;

	  renderBox(fruit_x, fruit_y, SSD1351_RED);
	} else {
	  int idx = dequeue(snake_body);
	  int cx = idx / SN_ROOM_SIZE;
	  int cy = idx % SN_ROOM_SIZE;

	  renderBox(cx, cy, SSD1351_BLACK);
	}

	int hit_self = 0;
	int* snake_parts = elements(snake_body);
	for (int i = 0; i < size(snake_body) - 1; i++) {
		  int x = snake_parts[i] / SN_ROOM_SIZE;
		  int y = snake_parts[i] % SN_ROOM_SIZE;
		  if (snake_x == x && snake_y == y) {
			  hit_self = 1;
			  break;
		  }
	}

	if (hit_self == 1) {
		initSnake();
		// RESET SNAKE AND SCORE !!!!
	}

	snprintf(send_buffer, BUFSIZE, "Score: %d", size(snake_body));
	SSD1351_WriteString(0, FOOTER_TOP + 4, send_buffer, Font_7x10, SSD1351_WHITE, SSD1351_BLACK);
}

void pongLoop() {

	SSD1351_FillRectangle(pg_ball->x, pg_ball->y, PG_UNIT, PG_UNIT, SSD1351_BLACK);

	SSD1351_FillRectangle((PG_MIN_X + PG_PADDLE_OFFSET) * PG_UNIT, (pg_paddle_player + PG_MIN_Y) * PG_UNIT, PG_UNIT, PG_PADDLE_LENGTH * PG_UNIT, SSD1351_BLACK);
	SSD1351_FillRectangle((PG_MAX_X - PG_PADDLE_OFFSET) * PG_UNIT, (pg_paddle_bot + PG_MIN_Y) * PG_UNIT, PG_UNIT, PG_PADDLE_LENGTH * PG_UNIT, SSD1351_BLACK);

	int direction = 0;
	uint32_t analogValue2 = readAnalog2();
	if (analogValue2 > JS_ZERO + JS_TOLERANCE)
	{
	  direction = -1;
	} else if (analogValue2 < JS_ZERO - JS_TOLERANCE)
	{
	  direction = 1;
	}

	if ((direction < 0 && pg_paddle_player > 0) || (direction > 0 && pg_paddle_player < PG_MAX_Y - PG_MIN_Y - PG_PADDLE_LENGTH + 1)) {
		pg_paddle_player += direction;
	}

	pg_ball->x = pg_ball->x + (PG_SPEED * PG_UNIT) * cos(pg_ball->direction);
	pg_ball->y = pg_ball->y + (PG_SPEED * PG_UNIT) * sin(pg_ball->direction);

	if (pg_ball->y >= PG_MAX_Y * PG_UNIT) {
		pg_ball->direction *= -1;
		pg_ball->y--;
	} else if (pg_ball->y <= 17) {
		pg_ball->direction *= -1;
		pg_ball->y++;
	}

	int bot_diff = pg_ball->y - ((pg_paddle_bot + PG_MIN_Y + PG_PADDLE_LENGTH / 2) * PG_UNIT);

	int bot_direction = bot_diff / abs(bot_diff);

	if ((bot_direction < 0 && pg_paddle_bot > 0) || (bot_direction > 0 && pg_paddle_bot < PG_MAX_Y - PG_MIN_Y - PG_PADDLE_LENGTH + 1)) {
		pg_paddle_bot += bot_direction;
	}

	int state = 0; // 0 - game, -1 - bot won, 1 - player won

	if ((pg_ball->x <= (PG_MIN_X + PG_PADDLE_OFFSET + 2) * PG_UNIT) && (pg_ball->x <= (PG_MIN_X + PG_PADDLE_OFFSET + 1) * PG_UNIT)) {
		if ((pg_ball->y >= (pg_paddle_player + PG_MIN_Y) * PG_UNIT) && (pg_ball->y <= (pg_paddle_player + PG_MIN_Y + PG_PADDLE_LENGTH) * PG_UNIT)) {
			pg_ball->direction -= M_PI_2;
			pg_ball->x+= PG_UNIT;
		} else {
			state = -1;
		}
	} else if ((pg_ball->x >= (PG_MAX_X - PG_PADDLE_OFFSET - 2) * PG_UNIT) && (pg_ball->x >= (PG_MAX_X - PG_PADDLE_OFFSET - 1) * PG_UNIT)) {
		if ((pg_ball->y >= (pg_paddle_bot + PG_MIN_Y) * PG_UNIT) && (pg_ball->y <= (pg_paddle_bot + PG_MIN_Y + PG_PADDLE_LENGTH) * PG_UNIT)) {
			pg_ball->direction -= M_PI_2;
			pg_ball->x-= PG_UNIT;
		} else {
			state = 1;
		}
	}

	SSD1351_FillRectangle(pg_ball->x, pg_ball->y, PG_UNIT, PG_UNIT, SSD1351_WHITE);

	SSD1351_FillRectangle((PG_MIN_X + PG_PADDLE_OFFSET) * PG_UNIT, (pg_paddle_player + PG_MIN_Y) * PG_UNIT, PG_UNIT, PG_PADDLE_LENGTH * PG_UNIT, SSD1351_WHITE);
	SSD1351_FillRectangle((PG_MAX_X - PG_PADDLE_OFFSET) * PG_UNIT, (pg_paddle_bot + PG_MIN_Y) * PG_UNIT, PG_UNIT, PG_PADDLE_LENGTH * PG_UNIT, SSD1351_WHITE);


	if (state != 0) {
		pg_score += state;

		SSD1351_FillRectangle(pg_ball->x, pg_ball->y, PG_UNIT, PG_UNIT, SSD1351_BLACK);

		pg_ball->x = 10 * PG_UNIT;
		pg_ball->y = 10 * PG_UNIT;
		pg_ball->direction = 2 * M_PI / 3;
	}

	snprintf(send_buffer, BUFSIZE, "Score: %3d", pg_score);
	SSD1351_WriteString(0, FOOTER_TOP + 4, send_buffer, Font_7x10, SSD1351_WHITE, SSD1351_BLACK);
}

void menuLoop() {
	uint32_t analogValue = readAnalog();
	uint32_t analogValue2 = readAnalog2();

	int prev_selected_icon_index = selected_icon_index;
	int diff1 = abs(analogValue - JS_ZERO);
	int diff2 = abs(analogValue2 - JS_ZERO);
	if (diff1 > diff2) {
		if (analogValue < JS_ZERO - JS_TOLERANCE && (selected_icon_index % 3) > 0)
		{
			selected_icon_index--;
		} else if (analogValue > JS_ZERO + JS_TOLERANCE && (selected_icon_index % 3) < 2)
		{
			selected_icon_index++;
		}
	}
	if (prev_selected_icon_index == selected_icon_index) {
		if (analogValue2 > JS_ZERO + JS_TOLERANCE && (selected_icon_index / 3) > 0)
		{
			selected_icon_index -= 3;
		} else if (analogValue2 < JS_ZERO - JS_TOLERANCE && (selected_icon_index / 3) < 1)
		{
			selected_icon_index += 3;
		}
	}

	snprintf(send_buffer, BUFSIZE, "Icon index: %d -> %d \r\n", prev_selected_icon_index, selected_icon_index);
	logSerial(send_buffer);

	if(prev_selected_icon_index != selected_icon_index) {
		logSerial("Updating menu");
		// deselect prev
		int x = ((prev_selected_icon_index % 3) + 1) * ICON_PADDING + (prev_selected_icon_index % 3) * ICON_WIDTH;
		int y = ((prev_selected_icon_index / 3) + 1 + (prev_selected_icon_index / 3 > 0 ? 1 : 0)) * ICON_PADDING + (prev_selected_icon_index / 3) * ICON_HEIGHT + HEADER_BOTTOM;
		SSD1351_FillRectangle(x - 1, y - 1, ICON_WIDTH + 2, 2, SSD1351_BLACK);
		SSD1351_FillRectangle(x - 1, y + ICON_HEIGHT - 1, ICON_WIDTH + 2, 2, SSD1351_BLACK);

		SSD1351_FillRectangle(x - 1, y + 1, 2, ICON_HEIGHT, SSD1351_BLACK);
		SSD1351_FillRectangle(x + ICON_WIDTH - 1, y + 1, 2, ICON_HEIGHT, SSD1351_BLACK);

		strokeRect(x, y, ICON_WIDTH, ICON_HEIGHT, SSD1351_RED);

		// select curr
		x = ((selected_icon_index % 3) + 1) * ICON_PADDING + (selected_icon_index % 3) * ICON_WIDTH;
		y = ((selected_icon_index / 3) + 1 + (selected_icon_index / 3 > 0 ? 1 : 0)) * ICON_PADDING + (selected_icon_index / 3) * ICON_HEIGHT + HEADER_BOTTOM;
		SSD1351_FillRectangle(x - 1, y - 1, ICON_WIDTH + 2, 2, SSD1351_BLUE);
		SSD1351_FillRectangle(x - 1, y + ICON_HEIGHT - 1, ICON_WIDTH + 2, 2, SSD1351_BLUE);

		SSD1351_FillRectangle(x - 1, y + 1, 2, ICON_HEIGHT, SSD1351_BLUE);
		SSD1351_FillRectangle(x + ICON_WIDTH - 1, y + 1, 2, ICON_HEIGHT, SSD1351_BLUE);


		char *name;
		switch(selected_icon_index) {
		case 0:
			name = "Snake";
			break;
		case 1:
			name = "Pong";
			break;
		default:
			name = "_";
		}

		SSD1351_FillRectangle(0, FOOTER_TOP, 127, 127, SSD1351_BLACK);
		snprintf(send_buffer, BUFSIZE, "Select game: %s", name);
	    SSD1351_WriteString(0, FOOTER_TOP + 4, send_buffer, Font_7x10, SSD1351_WHITE, SSD1351_BLACK);
	}
}

void render() {
	switch(curr_view) {
	case MENU:
		renderMenu();
		break;
	case SNAKE:
		renderSnake();
		break;
	case PONG:
		renderPong();
		break;
	// TODO default handler - log error
	}
}

void renderMenu() {
	SSD1351_FillRectangle(pg_ball->x, pg_ball->y, pg_ball->x + PG_UNIT, pg_ball->y + PG_UNIT, SSD1351_WHITE);
}

void renderBox(int x, int y, uint16_t color)
{
	for(int i = x * SN_UNIT; i < x * SN_UNIT + SN_UNIT; i++) {
		for(int j = y * SN_UNIT; j < y * SN_UNIT + SN_UNIT; j++) {
			SSD1351_DrawPixel(i, j, color);
		}
	}
}

void renderSnake()
{
    int body_size = size(snake_body);
    int* body_elements = elements(snake_body);
    for(int i = 0; i < body_size; i++)
    {
    	int idx = body_elements[i];
    	int cx = idx / SN_ROOM_SIZE;
    	int cy = idx % SN_ROOM_SIZE;
    	SSD1351_FillRectangle(cx, cy, cx + SN_UNIT, cy + SN_UNIT, SSD1351_WHITE);
    }

	SSD1351_FillRectangle(fruit_x, fruit_y, fruit_x + SN_UNIT, fruit_y + SN_UNIT, SSD1351_RED);

}

void renderPong()
{
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
  MX_I2C1_Init();
  MX_I2S3_Init();
  MX_SPI1_Init();
  MX_USB_DEVICE_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  init();
  while (1)
  {
	  loop();

	  // render();

	  if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_SET) {
		  switch (curr_view)  {
		  case MENU:
			  switch(selected_icon_index) {
			  case 0:
				  curr_view = SNAKE;
				  initSnake();
				  break;
			  case 1:
				  curr_view = PONG;
				  initPong();
				  break;
			  default:
				  snprintf(send_buffer, BUFSIZE, "Uncategorized option '%d'\r\n", selected_icon_index);
				  logSerial(send_buffer);
				  break;
			  }
			  break;
		  case SNAKE:
		  case PONG:
			  curr_view = MENU;
			  initMenu();
			  break;
		  default:
			  break;
		  }
	  }


	  //snprintf(send_buffer, BUFSIZE, "Analog %d. X: %d, Y: %d \r\n", analogValue, snake_x, snake_y);
	  //CDC_Transmit_FS(send_buffer, strlen(send_buffer));

	  HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);
	  HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
	  HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_14);
	  HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);

	  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5); // External LED on PB5

	  key_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4); // External Key on PB4

	  snprintf(send_buffer, BUFSIZE, "Hello World [%d]: Key: %d \r\n", counter++, key_state);
	  CDC_Transmit_FS(send_buffer, strlen(send_buffer));

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  HAL_Delay(100);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
  PeriphClkInitStruct.PLLI2S.PLLI2SN = 192;
  PeriphClkInitStruct.PLLI2S.PLLI2SR = 2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_10B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_10B;
  hadc2.Init.ScanConvMode = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2S3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S3_Init(void)
{

  /* USER CODE BEGIN I2S3_Init 0 */

  /* USER CODE END I2S3_Init 0 */

  /* USER CODE BEGIN I2S3_Init 1 */

  /* USER CODE END I2S3_Init 1 */
  hi2s3.Instance = SPI3;
  hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_96K;
  hi2s3.Init.CPOL = I2S_CPOL_LOW;
  hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S3_Init 2 */

  /* USER CODE END I2S3_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : CS_I2C_SPI_Pin */
  GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PDM_OUT_Pin */
  GPIO_InitStruct.Pin = PDM_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BOOT1_Pin PB4 */
  GPIO_InitStruct.Pin = BOOT1_Pin|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : CLK_IN_Pin */
  GPIO_InitStruct.Pin = CLK_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB12 PB13 PB14 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
                           Audio_RST_Pin */
  GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MEMS_INT2_Pin */
  GPIO_InitStruct.Pin = MEMS_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
static uint32_t readAnalog(void)
{
	// Start ADC Conversion
	  HAL_ADC_Start(&hadc1);
	  // Poll ADC1 Perihperal & TimeOut = 1mSec
	  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	  // Read The ADC Conversion Result & Map It To PWM DutyCycle
	  uint32_t value = HAL_ADC_GetValue(&hadc1);
	  // TIM2->CCR1 = (AD_RES<<4);
	  HAL_Delay(1);

	  return value;
}

static uint32_t readAnalog2(void)
{
	// Start ADC Conversion
	  HAL_ADC_Start(&hadc2);
	  // Poll ADC1 Perihperal & TimeOut = 1mSec
	  HAL_ADC_PollForConversion(&hadc2, HAL_MAX_DELAY);
	  // Read The ADC Conversion Result & Map It To PWM DutyCycle
	  uint32_t value = HAL_ADC_GetValue(&hadc2);
	  // TIM2->CCR1 = (AD_RES<<4);
	  HAL_Delay(1);

	  return value;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
