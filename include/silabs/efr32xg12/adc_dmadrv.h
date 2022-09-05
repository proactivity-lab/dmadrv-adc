/**
 * Simplified ping-pong ADC stream sampling with DMA and PRS.
 *
 * Copyright ProLab 2020
 * @license MIT
 */
#ifndef ADC_DMADRV_H_
#define ADC_DMADRV_H_

#include "em_adc.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * Data callback.
 * @param data  - pointer to the data buffer.
 * @param count - number of samples in the buffer.
 * @param user  - user pointer.
 * @return New buffer to continue sampling or NULL to stop.
 */
typedef uint16_t * (*dmadrv_adc_cb_f)(uint16_t * data, uint16_t count, void * user);

/**
 * Initialize the sampling engine.
 * @param count     - sample count (buffers must be of this size at least)
 * @param frequency - sampling frequency, Hz, > 0.
 * @param cb        - Data callback.
 * @param user      - User pointer given to data callback.
 */
void dmadrv_adc_init(uint16_t count, uint16_t frequency, dmadrv_adc_cb_f cb, void * user);

/**
 * Add an input to be sampled. Follow em_adc SCAN mode rules from datasheet for setup.
 * @param group - Input group.
 * @param sel   - Input to put in the group.
 */
void dmadrv_adc_add_input(ADC_ScanInputGroup_TypeDef group, ADC_PosSel_TypeDef sel);

/**
 * Start sampling.
 * @param data Initial data buffer to sample into, must be >= the size given to init.
 * @return true if sampling started.
 */
bool dmadrv_adc_start(uint16_t * data);

/**
 * Stop sampling.
 * @return true if sampling stopped.
 */
bool dmadrv_adc_stop();

/**
 * Deinitialize and free resources.
 */
void dmadrv_adc_deinit();

#endif//ADC_DMADRV_H_
