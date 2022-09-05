/**
 * Simplified ping-pong ADC stream sampling with DMA and PRS.
 *
 * Copyright ProLab 2020
 * @license MIT
 */
#include "adc_dmadrv.h"

#include "dmadrv.h"
#include "em_prs.h"
#include "em_adc.h"
#include "em_cmu.h"
#include "em_timer.h"

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "loglevels.h"
#define __MODUUL__ "adcd"
#define __LOG_LEVEL__ (LOG_LEVEL_adc_dmadrv & BASE_LOG_LEVEL)
#include "log.h"
#include "sys_panic.h"

static unsigned int m_dma_channel;
static int m_prs_channel;

static uint16_t m_count;
static uint16_t m_frequency;
static dmadrv_adc_cb_f m_cbf;
static void * m_user;

static ADC_InitScan_TypeDef m_scan;

static bool dma_setup(uint16_t * buf, uint16_t count);

void dmadrv_adc_init(uint16_t count, uint16_t frequency, dmadrv_adc_cb_f cb, void * user)
{
	Ecode_t result = DMADRV_AllocateChannel(&m_dma_channel, NULL);
	if (ECODE_OK != result)
	{
		sys_panic("dma");
	}

	m_count = count;
	m_frequency = frequency;
	m_cbf = cb;
	m_user = user;

	// Scan mode setup - use SCAN mode even if sampling a single channel
	m_scan = (ADC_InitScan_TypeDef)ADC_INITSCAN_DEFAULT;
}

void dmadrv_adc_deinit()
{
	DMADRV_FreeChannel(m_dma_channel);
}

void dmadrv_adc_add_input(ADC_ScanInputGroup_TypeDef inputGroup, ADC_PosSel_TypeDef singleEndedSel)
{
	ADC_ScanSingleEndedInputAdd(&m_scan, inputGroup, singleEndedSel);
}

static bool dmadrv_callback(unsigned int channel, unsigned int sequenceNo, void *data)
{
	uint16_t * newdata = m_cbf((uint16_t*)data, m_count, m_user);
	if(NULL != newdata) // Restart DMA with new buffer
	{
		if(dma_setup(newdata, m_count))
		{
			return false; // Not using built-in ping-pong, so return false
		}
	}

	dmadrv_adc_stop(); // Nowhere to put data or failure -> STOP
	return false;
}

static bool dma_setup(uint16_t * data, uint16_t count)
{
	Ecode_t err = DMADRV_PeripheralMemory(m_dma_channel, dmadrvPeripheralSignal_ADC0_SCAN,
	                                      data, (void*)&(ADC0->SCANDATA),
	                                      true, m_count, dmadrvDataSize2,
	                                      dmadrv_callback, data);
	return err == ECODE_OK;
}

static void adc_setup(int prs_channel)
{
	ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
	// Request 1 MHz for ADC clock ... TODO determine optimal value
	init.prescale = ADC_PrescaleCalc(1000000, 0);
	debug1("adc prescale %u", init.prescale);

	CMU_ClockEnable(cmuClock_ADC0, true);

	m_scan.prsSel = prs_channel;
	m_scan.reference = adcRefVDD; // adcRef5V
	m_scan.prsEnable = true;
	m_scan.fifoOverwrite = true;

	ADC_InitScan(ADC0, &m_scan);
	ADC_Init(ADC0, &init);

	// Clear any pending interrupts
	ADC0->SCANFIFOCLEAR = ADC_SCANFIFOCLEAR_SCANFIFOCLEAR;
}

static TIMER_Prescale_TypeDef calculate_prescaler(uint16_t * value, uint16_t frequency)
{
	// TIMER1 is 16-bits, find suitable prescaler to use as many bits as possible
	uint16_t prescaler = CMU_ClockFreqGet(cmuClock_TIMER1) / UINT16_MAX / m_frequency;
	uint16_t p = 0;
	while(0 != prescaler)
	{
		prescaler >>= 1;
		p++;
	}

	switch(p) // A switch, because the constants are now powers of 2 in all cases
	{
		case 0:
			*value = 1;
			return timerPrescale1;
		case 1:
			*value = 2;
			return timerPrescale2;
		case 2:
			*value = 4;
			return timerPrescale4;
		case 3:
			*value = 8;
			return timerPrescale8;
		case 4:
			*value = 16;
			return timerPrescale16;
		case 5:
			*value = 32;
			return timerPrescale32;
		case 6:
			*value = 64;
			return timerPrescale64;
		case 7:
			*value = 128;
			return timerPrescale128;
		case 8:
			*value = 256;
			return timerPrescale256;
		case 9:
			*value = 512;
			return timerPrescale512;
		case 10:
			*value = 1024;
			return timerPrescale1024;
		default:
			err1("p %"PRIu16":%"PRIu16, p, prescaler); // Timing will be incorrect
			*value = 1024;
			return timerPrescale1024;
	}
}

static void timer_setup(uint16_t frequency)
{
	uint16_t prescale = 1;
	TIMER_Init_TypeDef timerInit = {
		.enable = false,
		.debugRun = false,
		.prescale = calculate_prescaler(&prescale, frequency), // timerPrescale1 - timerPrescale1024
		.clkSel = timerClkSelHFPerClk,
		.fallAction = timerInputActionNone,
		.riseAction = timerInputActionNone,
		.mode = timerModeUp,
		.dmaClrAct = false,
		.quadModeX4 = false,
		.oneShot = false,
		.sync = false,
	};

	CMU_ClockEnable(cmuClock_TIMER1, true);
	TIMER_Init(TIMER1, &timerInit);

	uint32_t top = CMU_ClockFreqGet(cmuClock_TIMER1)/(prescale*frequency);
	if(top > UINT16_MAX)
	{
		err1("top %"PRIu32, top);
		top = UINT16_MAX;
	}
	debug1("prescale %"PRIu16" top %"PRIu32, prescale, top);

	TIMER_TopSet(TIMER1,  top);

	TIMER_Enable(TIMER1, true);
}

bool dmadrv_adc_start(uint16_t * data)
{
	if(0 == m_frequency)
	{
		return false;
	}
	if(0 > m_dma_channel)
	{
		return false;
	}

	// PRS
	CMU_ClockEnable(cmuClock_PRS, true);
	m_prs_channel = PRS_GetFreeChannel(prsTypeAsync);

	if(m_prs_channel < 0)
	{
		err1("prs");
		return false;
	}

	// Timer overflow should trigger sampling
	PRS_SourceSignalSet(m_prs_channel, PRS_CH_CTRL_SOURCESEL_TIMER1, PRS_CH_CTRL_SIGSEL_TIMER1OF, prsEdgeOff);

	debug1("dma %u prs %d", m_dma_channel, m_prs_channel);

	adc_setup(m_prs_channel);

	if(false == dma_setup(data, m_count))
	{
		err1("dma");
		return false;
	}

	timer_setup(m_frequency);

	return true;
}

// finish running transfer
bool dmadrv_adc_stop()
{
	TIMER_Enable(TIMER1, false);
	DMADRV_StopTransfer(m_dma_channel);
	ADC_Reset(ADC0);
	PRS_SourceSignalSet(m_prs_channel, PRS_CH_CTRL_SOURCESEL_NONE, PRS_CH_CTRL_SIGSEL_TIMER1OF, prsEdgeOff);
	return false;
}
