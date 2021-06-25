/* 
 * File:   oc1.h
 * Author: Diogo Vala
 *
 * Overview: Configure OC1 module
 */

#ifndef OC1_H
#define OC1_H

#include <stdint.h>

// Define return codes
#define OC1_SUCCESS 0;
#define OC1_CLOCK_SOURCE_NOT_SUP -1;
#define OC1_NOT_CONFIGURED -2;

#define oc1START 1
#define oc1STOP 0
#define oc1MAX_DUTYCYCLE 254

/********************************************************************
 * Function: 	 OC1Config()
 * Precondition: Source timer should be previously configured.
 * Input: 		 timer - 2 for Timer2 ; 3 for Timer3
 * Returns:      OC1_SUCCESS if configuration successful.
 *               OC1_XXX error codes in case of failure.
 * Side Effects: OC mode is set to 6 by default or must be manually
 *               configured.
 * Overview:     Configures OC1 module.
 ********************************************************************/
int8_t OC1Config(uint8_t timer);

/********************************************************************
 * Function: 	 OC1Control()
 * Precondition: OC1 should be previously configured.
 * Input: 		 oc1run : {1 start - 0 stop}         
 * Overview:     Starts/Stops OC1
 ********************************************************************/
void OC1Control(uint8_t oc1run);

/********************************************************************
 * Function: 	 OC1SetDutyCycle()
 * Precondition: Timer should be previously configured
 * Input: 		 dutycycle {0-65535}
 * Returns:      OC1_NOT_CONFIGURED if OC1 not configured correctly 
 *               before calling this function.        
 * Overview:     
 * Note:		 The dutycycle range can be adjusted manually
 ********************************************************************/
int8_t OC1SetDutyCycle(uint16_t dutycycle);

#endif
