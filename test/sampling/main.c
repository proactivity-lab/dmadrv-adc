/**
 * Fast ADC sampling demo-app.
 *
 * Copyright ProLab 2020
 * @license MIT
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#include "em_chip.h"
#include "em_rmu.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_msc.h"
#include "dmadrv.h"

#include "adc_dmadrv.h"

#include "retargetserial.h"
#include "loggers_ext.h"

#include "platform.h"

#include "incbin.h"
#include "SignatureArea.h"
#include "DeviceSignature.h"

#include "loglevels.h"
#define __MODUUL__ "main"
#define __LOG_LEVEL__ (LOG_LEVEL_main & BASE_LOG_LEVEL)
#include "log.h"

// Add the headeredit block
#include "incbin.h"
INCBIN(Header, "header.bin");

static int logger_fwrite_boot (const char *ptr, int len)
{
	fwrite(ptr, len, 1, stdout);
	fflush(stdout);
	return len;
}

static volatile bool m_stuffdone = false;

static uint16_t m_data1[200];
static uint16_t m_data2[200];

static uint16_t * adc_callback(uint16_t * data, uint16_t count, void * user)
{
	m_stuffdone = true;

	if(data == m_data1)
	{
		return m_data2;
	}
	return NULL;
}

int main()
{
	PLATFORM_Init(); // Does CHIP_Init() and MSC_Init(), returns resetCause

	// LED
	PLATFORM_LedsInit();
	PLATFORM_LedsSet(2);

	RETARGET_SerialInit();
	log_init(BASE_LOG_LEVEL, &logger_fwrite_boot, NULL);

    debug1("DMA ADC Demo "VERSION_STR" (%d.%d.%d)", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

	sigInit();

	uint8_t eui[8];
	sigGetEui64(eui);
	infob1("EUI64:", eui, sizeof(eui));

	debug1("test");

	GPIO_PinModeSet(gpioPortA, 0, gpioModeInput, 0);
	GPIO_PinModeSet(gpioPortA, 1, gpioModePushPull, 0);

	DMADRV_Init();

	for(int i=0;i<10;i++)
	{
		dmadrv_adc_init(100, 3000, &adc_callback, NULL);

		// Add channels
		dmadrv_adc_add_input(adcScanInputGroup0, adcPosSelAPORT3XCH8);   // PA0
		//dmadrv_adc_add_input(adcScanInputGroup0, adcPosSelAPORT3YCH9); // PA1

		dmadrv_adc_start(m_data1);

		while(false == m_stuffdone);
		m_stuffdone = false;

		debug1("data1");
		for(int i=0;i<100;i++)
		{
			debug1("%02d: %"PRIu16, i, m_data1[i]);
		}
		while(false == m_stuffdone);

		m_stuffdone = false;
		debug1("data2");
		for(int i=0;i<100;i++)
		{
			debug1("%02d: %"PRIu16, i, m_data2[i]);
		}

		dmadrv_adc_stop();

		debug1("done");

		dmadrv_adc_deinit();

		for(int d=0;d<10000000;d++) __asm__("nop");
	}

	while(1);
}
