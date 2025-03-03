/* main.cpp
*
*  MIT License
*
*  Copyright (c) 2023-2025 awawa-dev
*
*  https://github.com/awawa-dev/HyperSerialPico
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#define TUD_OPT_HIGH_SPEED


#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <algorithm>
#include "pico/stdio/driver.h"
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "leds.h"


///////////////////////////////////////////////////////////////////////////
// DO NOT EDIT THIS FILE. ADJUST THE CONFIGURATION IN THE CmakeList.txt  //
///////////////////////////////////////////////////////////////////////////

#define _STR(x) #x
#define _XSTR(x) _STR(x)
#define VAR_NAME_VALUE(var) #var " = " _XSTR(var)
#define _XSTR2(x,y) _STR(x) _STR(y)
#define VAR_NAME_VALUE2(var) #var " = " _XSTR2(var)

#if defined(BOOT_WORKAROUND) && defined(PICO_XOSC_STARTUP_DELAY_MULTIPLIER)
	#pragma message("Enabling boot workaround")
	#pragma message(VAR_NAME_VALUE(PICO_XOSC_STARTUP_DELAY_MULTIPLIER))
#endif


#ifdef NEOPIXEL_RGBW
	#pragma message(VAR_NAME_VALUE(NEOPIXEL_RGBW))
#endif
#ifdef NEOPIXEL_RGB
	#pragma message(VAR_NAME_VALUE(NEOPIXEL_RGB))
#endif
#ifdef COLD_WHITE
	#pragma message(VAR_NAME_VALUE(COLD_WHITE))
#endif
#ifdef SPILED_APA102
	#pragma message(VAR_NAME_VALUE(SPILED_APA102))
#endif

#ifdef NEOPIXEL_RGBW
	#define LED_DRIVER sk6812
#elif NEOPIXEL_RGB
	#define LED_DRIVER ws2812
#endif

#ifdef SPILED_APA102
	#define LED_DRIVER apa102
	#pragma message(VAR_NAME_VALUE(SPI_INTERFACE))
#endif

	#pragma message(VAR_NAME_VALUE(DATA_PIN))
#ifdef CLOCK_PIN
	#pragma message(VAR_NAME_VALUE(CLOCK_PIN))
#endif

#if defined(SECOND_SEGMENT_START_INDEX)
	#pragma message("Using parallel mode for segments")

	#ifdef NEOPIXEL_RGBW
			#undef LED_DRIVER
			#define LED_DRIVER sk6812p
			#define LED_DRIVER2 sk6812p
	#elif NEOPIXEL_RGB
			#undef LED_DRIVER
			#define LED_DRIVER ws2812p
			#define LED_DRIVER2 ws2812p
	#else
		#error "Parallel mode is unsupportd for selected LEDs configuration"
	#endif

	#pragma message(VAR_NAME_VALUE(LED_DRIVER))
	#pragma message(VAR_NAME_VALUE(SECOND_SEGMENT_START_INDEX))
	#pragma message(VAR_NAME_VALUE(LED_DRIVER2))
	#pragma message(VAR_NAME_VALUE(SECOND_SEGMENT_REVERSED))
#else
	#pragma message(VAR_NAME_VALUE(LED_DRIVER))

	typedef LedDriver LED_DRIVER2;
#endif

/////////////////////////////////////////////////////////////////////////
#define delay(x) sleep_ms(x)
#define yield() busy_wait_us(100)
#define millis xTaskGetTickCount

#include "main.h"

static void core1()
{
    for( ;; )
    {
        if (sem_acquire_timeout_us(&base.serialSemaphore, portMAX_DELAY))
        {
            processData();
        }
    }
}

static void core0( void *pvParameters )
{
    for( ;; )
    {
        if (sem_acquire_timeout_us(&base.receiverSemaphore, portMAX_DELAY))
        {
            int wanted, received;
            do
            {
                wanted = std::min(MAX_BUFFER - base.queueEnd, MAX_BUFFER - 1);
                received = stdio_usb.in_chars((char*)(&(base.buffer[base.queueEnd])), wanted);
                if (received > 0)
                {
                    base.queueEnd = (base.queueEnd + received) % (MAX_BUFFER);
                }
            }while(wanted == received);

            sem_release(&base.serialSemaphore);
        }
    }
}

static void serialEvent(void *)
{
    sem_release(&base.receiverSemaphore);
}

int main(void)
{
    stdio_init_all();

    sem_init(&base.serialSemaphore, 0, 1);

    sem_init(&base.receiverSemaphore, 0, 1);

    multicore_launch_core1(core1);

    stdio_set_chars_available_callback(serialEvent, nullptr);

    xTaskCreate(core0,
            "HyperSerialPico:core0",
            configMINIMAL_STACK_SIZE * 2,
            NULL,
            (configMAX_PRIORITIES - 1),
            &base.processSerialHandle);

    vTaskStartScheduler();
    panic_unsupported();
}
