/* 
 * 28-May-2021
 * Beatriz Silva 85276
 * Diogo Vala 64671
 * 
 * ADC Module
 */

#pragma once

#include <stdint.h>

#define adcADC_SUCCESS 0
#define adcADC_INVALID_NUM_OF_SAMPLES -1
#define adcADC_INVALID_PIN -2;

/********************************************************************
 * Function: 	 adcConfig()
 * Precondition: Must configure TRIS of Analog pin
 * Input: 		 nSamples and AnalogPin
 * Returns:      ADC_SUCCESS if configuration successful.
 *               ADC_XXX error codes in case of failure
 * Side Effects: 
 *               
 * Overview:     Configures ADC module.
 *		
 * Note:		 
 * 
 ********************************************************************/
int8_t adcConfig(uint8_t nSamples, uint8_t AnalogPin);



/********************************************************************
 * Function: 	 adcControl()
 * Precondition: ADC must be previously configured
 * Input: 		 adcrun {0-disable; 1-enable}
 * Returns:      
 * Side Effects: 
 *               
 * Overview:     Enabled/Disables ADC module.
 *		
 * Note:		 
 * 
 ********************************************************************/
int8_t adcControl(uint8_t adcrun);



/********************************************************************
 * Function: 	 getADCsample()
 * Precondition: ADC must be previously configured
 * Input: 		 
 * Returns:      Acquired sample ( 0 - 1023 )
 * Side Effects: 
 *               
 * Overview:     Resets flag, starts conversion, waits for EOC and
 *               returns sample
 *		
 * Note:		 
 * 
 ********************************************************************/
uint16_t getADCsample();

