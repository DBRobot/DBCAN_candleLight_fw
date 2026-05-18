#include "board.h"
#include "config.h"
#include "device.h"
#include "gpio.h"
#include "stm32g0xx_hal_gpio.h"
#include "usbd_gs_can.h"

static void dbcan_setup(USBD_GS_CAN_HandleTypeDef *hcan) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    UNUSED(hcan);

    __HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/* Dummy LED pin (no physical LED, just satisfies the API) */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = 0;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* TERM pins */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = 0;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_15;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* FDCAN */

    RCC_PeriphCLKInitTypeDef PeriphClkInit = {
		.PeriphClockSelection = RCC_PERIPHCLK_FDCAN,
		.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL,
	};

    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
	__HAL_RCC_FDCAN_CLK_ENABLE();

	/* FDCAN1_RX, FDCAN1_TX */
	GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF3_FDCAN1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* FDCAN2_RX, FDCAN2_TX */
	GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF3_FDCAN2;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void dbcan_termination_set(can_data_t *channel, enum gs_can_termination_state enable) 
{
    const uint8_t nr = channel->nr;

	if (nr == 0) {
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}
    
}

const struct board_config config = {
	.setup = dbcan_setup,
	SET_TERMINATION_FN(dbcan_termination_set)
	.channel[0] = {
		.interface = FDCAN2,
		.leds = {
			[LED_RX] = { .port = GPIOA, .pin = GPIO_PIN_1, .active_high = 0 },
			[LED_TX] = { .port = GPIOA, .pin = GPIO_PIN_1, .active_high = 0 },
		},
	},
	.channel[1] = {
		.interface = FDCAN1,
		.leds = {
			[LED_RX] = { .port = GPIOA, .pin = GPIO_PIN_1, .active_high = 0 },
			[LED_TX] = { .port = GPIOA, .pin = GPIO_PIN_1, .active_high = 0 },
		},
	},
};