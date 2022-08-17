/**
 * Simplified ping-pong ADC stream sampling with DMA and PRS.
 *
 * Copyright ProLab 2020
 * @license MIT
 */
#include "adc_dmadrv.h"

#include "dmadrv.h"
#include "em_prs.h"
#include "em_iadc.h"
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

#define IADC_INPUT_0_PORT_PIN     iadcPosInputPortBPin1;

// When changing GPIO port/pins above, make sure to change xBUSALLOC macro's
// accordingly.
#define IADC_INPUT_0_BUS          BBUSALLOC
#define IADC_INPUT_0_BUSALLOC     GPIO_BBUSALLOC_BODD0_ADC0

#define CLK_SRC_ADC_FREQ          1000000 // CLK_SRC_ADC
#define CLK_ADC_FREQ              100000 // CLK_ADC

// Set HFRCOEM23 to lowest frequency (1MHz)
#define HFRCOEM23_FREQ            cmuHFRCOEM23Freq_1M0Hz

static unsigned int m_dma_channel;
static int m_prs_channel;

static uint16_t m_count;
static uint16_t m_frequency;
static dmadrv_adc_cb_f m_cbf;
static void * m_user;

//static IADC_InitScan_TypeDef m_scan;

static bool dma_setup(uint16_t * buf, uint16_t count);

void dmadrv_adc_init(uint16_t count, uint16_t frequency, dmadrv_adc_cb_f cb, void * user)
{
	debug1("dmadrv adc init");
	Ecode_t result = DMADRV_AllocateChannel(&m_dma_channel, NULL);
	if (ECODE_OK != result)
	{
		sys_panic("dma");
	}

	m_count = count;
	m_frequency = frequency;
	m_cbf = cb;
	m_user = user;
}

void dmadrv_adc_deinit()
{
	DMADRV_FreeChannel(m_dma_channel);
}

void dmadrv_adc_add_input(void)
{
	//ADC_ScanSingleEndedInputAdd(&m_scan, inputGroup, singleEndedSel);
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
	Ecode_t err = DMADRV_PeripheralMemory(m_dma_channel, dmadrvPeripheralSignal_IADC0_IADC_SINGLE,
	                                      data, (void*)&(IADC0->SINGLEFIFODATA),
	                                      true, m_count, dmadrvDataSize2,
	                                      dmadrv_callback, data);
	return err == ECODE_OK;
}

static void adc_setup(void)
{
  // Declare init structs
  IADC_Init_t init = IADC_INIT_DEFAULT;
  IADC_AllConfigs_t initAllConfigs = IADC_ALLCONFIGS_DEFAULT;
  IADC_InitSingle_t initSingle = IADC_INITSINGLE_DEFAULT;
  IADC_SingleInput_t initSingleInput = IADC_SINGLEINPUT_DEFAULT;

  // Reset IADC to reset configuration in case it has been modified
  IADC_reset(IADC0);

  CMU_HFRCOEM23BandSet(HFRCOEM23_FREQ);
  // Configure IADC clock source for use while in EM2
  CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_HFRCOEM23); // 1MHz

  CMU_ClockEnable(cmuClock_IADCCLK, true);

  // Modify init structs and initialize
  init.warmup = iadcWarmupNormal;

  // Set the HFSCLK prescale value here
  init.srcClkPrescale = IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);

  // Set timer cycles to configure sampling rate
  init.timerCycles = 337;

  // Configuration 0 is used by both scan and single conversions by default
  // Use unbuffered AVDD as reference
  initAllConfigs.configs[0].reference = iadcCfgReferenceVddx;
  initAllConfigs.configs[0].analogGain = iadcCfgAnalogGain1x;

  // Divides CLK_SRC_ADC to set the CLK_ADC frequency
  // Default oversampling (OSR) is 2x,
  //  and Conversion Time = ((4 * OSR) + 2) / fCLK_ADC
  initAllConfigs.configs[0].adcClkPrescale = IADC_calcAdcClkPrescale(IADC0,
                                                          CLK_ADC_FREQ,
                                                          0,
                                                          iadcCfgModeNormal,
                                                          init.srcClkPrescale);

  // Single initialization
  initSingle.triggerSelect = iadcTriggerSelTimer;
  initSingle.dataValidLevel = _IADC_SINGLEFIFOCFG_DVL_VALID1;

  // Enable triggering of single conversion
  initSingle.start = true;

  // Configure Input sources for single ended conversion
  initSingleInput.posInput = IADC_INPUT_0_PORT_PIN;
  initSingleInput.negInput = iadcNegInputGnd;

  // Initialize IADC
  IADC_init(IADC0, &init, &initAllConfigs);

  // Initialize Single
  IADC_initSingle(IADC0, &initSingle, &initSingleInput);

  // Allocate the analog bus for ADC0 inputs
  GPIO->IADC_INPUT_0_BUS |= IADC_INPUT_0_BUSALLOC;
}


bool dmadrv_adc_start(uint16_t * data)
{
	debug1("dmadrv adc start");
	if(0 == m_frequency)
	{
		return false;
	}
	if(0 > m_dma_channel)
	{
		return false;
	}

	adc_setup();

	if(false == dma_setup(data, m_count))
	{
		err1("dma");
		return false;
	}

	IADC_command(IADC0, iadcCmdEnableTimer);

	return true;
}

// finish running transfer
bool dmadrv_adc_stop()
{
	DMADRV_StopTransfer(m_dma_channel);
	IADC_reset(IADC0);
	return false;
}
