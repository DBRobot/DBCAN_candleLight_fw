/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Hubert Denkmair
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdint.h>

#include "dfu.h"
#include "gpio.h"

#define RESET_TO_BOOTLOADER_MAGIC_CODE 0xDEADBEEF

#define SYSMEM_STM32F042			   0x1FFFC400
#define SYSMEM_STM32F072			   0x1FFFC800
#define SYSMEM_STM32G0B1			   0x1FFF0000
#define SYSMEM_STM32G4xx			   0x1FFF0000

/* See __initialize_hardware_early — these two live in .bss but are read
 * BEFORE the copy table zeros .bss, so their RAM contents survive a soft
 * reset. The clean_boot_marker is set in our forced re-reset path so the
 * second boot recognizes itself and breaks the loop. */
#define CLEAN_BOOT_MAGIC               0xC0FFEE5AU
static uint32_t dfu_reset_to_bootloader_magic;
static uint32_t clean_boot_marker;

static void dfu_hack_boot_pin_f042(void)
{
	__HAL_RCC_GPIOF_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct = {
		.Pin = GPIO_PIN_11,
		.Mode = GPIO_MODE_OUTPUT_PP,
		.Pull = GPIO_PULLUP,
		.Speed = GPIO_SPEED_FREQ_LOW,
	};
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_SET);
}

static void dfu_jump_to_bootloader(uint32_t sysmem_base)
{
	void (*bootloader)(void) = (void (*)(void))(*((uint32_t *)(sysmem_base + 4)));

	__set_MSP(*(__IO uint32_t*)sysmem_base);
	bootloader();

	while (42) {
	}
}

void __initialize_hardware_early(void)
{
	if (dfu_reset_to_bootloader_magic == RESET_TO_BOOTLOADER_MAGIC_CODE) {
		switch (HAL_GetDEVID()) {
			case 0x445: // STM32F04x
				dfu_hack_boot_pin_f042();
				dfu_jump_to_bootloader(SYSMEM_STM32F042);
				break;

			case 0x448: // STM32F07x
				dfu_jump_to_bootloader(SYSMEM_STM32F072);
				break;

			case 0x467: // STM32G0B1
				dfu_jump_to_bootloader(SYSMEM_STM32G0B1);
				break;

			case 0x468: // STM32G4xx
				dfu_jump_to_bootloader(SYSMEM_STM32G4xx);
				break;
		}
	}

	/* Double-reset trick to escape stale state left by the ROM DFU
	 * bootloader (or anything else) before user firmware runs.
	 *
	 * The ROM bootloader's NVIC_SystemReset on `:leave` doesn't fully
	 * clear the USB peripheral / clock-tree state on STM32G0B1 — host
	 * enumeration then fails at SET_ADDRESS until a real power cycle.
	 * By issuing our own NVIC_SystemReset here, before any peripheral
	 * touches user code, the second boot starts from a reset we own
	 * and the chip behaves identically to POR.
	 */
	if (clean_boot_marker != CLEAN_BOOT_MAGIC) {
		clean_boot_marker = CLEAN_BOOT_MAGIC;
		NVIC_SystemReset();
	}
	clean_boot_marker = 0U;  /* clear so subsequent power-cycle (random RAM) triggers the workaround again */

	SystemInit();
}

void dfu_run_bootloader(void)
{
	dfu_reset_to_bootloader_magic = RESET_TO_BOOTLOADER_MAGIC_CODE;
	NVIC_SystemReset();
}
