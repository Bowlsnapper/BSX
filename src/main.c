/*
SPDX short identifier: BSD-2-Clause
BSD 2-Clause License

Copyright (c) 2024, Shalx <Alejandro L. Huitron shalxmva@gmail.com>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "modxo/modxo.h"
#include "modxo/text_display/hd44780.h"
#include "modxo_pinout.h"

#define SYS_FREQ_IN_KHZ (264 * 1000)

MODXO_TD_DRIVER_T text_display_driver;
bool reset_pin = false;

void core1_main()
{
    while (true)
    {
        modxo_poll_core1();
        __wfe();
    }
}

void core0_main()
{
    while (true)
    {
        modxo_poll_core0();
        __wfe();
    }
}

void reset_pin_falling()
{
    modxo_lpc_reset();
}

void pin_3_3v_falling()
{
    modxo_low_power_mode();
}

void core0_irq_handler(uint gpio, uint32_t event)
{
    if (gpio == LPC_RESET && (event & GPIO_IRQ_EDGE_FALL) != 0)
    {
        reset_pin_falling();
    }

    if (gpio == LPC_ON && (event & GPIO_IRQ_EDGE_FALL) != 0)
    {
        pin_3_3v_falling();
    }
}

void xbox_shutdown()
{
    multicore_reset_core1();
}

void modxo_init_pin_irq_fall(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_FALL, true);
}

void modxo_init_interrupts()
{
    modxo_init_pin_irq_fall(LPC_RESET);
    modxo_init_pin_irq_fall(LPC_ON);

    gpio_set_irq_callback(core0_irq_handler);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

int main(void)
{
    set_sys_clock_khz(SYS_FREQ_IN_KHZ, true);
    stdio_init_all();

#ifdef START_DELAY
    sleep_ms(2000);
#endif
    gpio_init(LED_STATUS_PIN);
    gpio_set_dir(LED_STATUS_PIN, GPIO_OUT);
    gpio_put(LED_STATUS_PIN, 1);

    hd44780_i2c_init(&text_display_driver, 0x27, HD44780_LCDTYPE_20X4); // Set your LCD Type, TODO: NVM parameter config
    modxo_init_interrupts();
    modxo_init(&text_display_driver);

    multicore_reset_core1();
    multicore_launch_core1(core1_main);

    core0_main(); // Infinite loop
}