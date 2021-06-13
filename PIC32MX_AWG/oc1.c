/* 
 * File:   oc1.c
 * Author: Diogo Vala
 *
 * Overview: Configures OC1 module
 */

#include <xc.h>
#include <stdlib.h>
#include <math.h>
#include "oc1.h"

uint8_t PRx;
int8_t OC1Config(uint8_t timer){

    if (timer != 2 && timer != 3) {
        return OC1_CLOCK_SOURCE_NOT_SUP;
    } 
    else 
    {
        PRx = timer;
        OC1CONbits.OCM = 6; // OCM = 0b110 : OC1 in PWM mode,
        OC1CONbits.OCTSEL = timer-2; // Timer is clock source of OCM
        OC1RS = 0; // Initial OC1R value
        return OC1_SUCCESS;
    }
}

void OC1Control(uint8_t oc1run){
    if (oc1run) {
        OC1CONbits.ON = 1; // Enable OC1
    } else {
        OC1CONbits.ON = 0; // Disable OC1
    }
}

int8_t OC1SetDutyCycle(uint16_t dutycycle){
    if (PRx == 2) {
        OC1RS = (PR2) * dutycycle / oc1MAX_DUTYCYCLE;
    } else if (PRx == 3) {
        OC1RS = (PR3) * dutycycle / oc1MAX_DUTYCYCLE;
    } else {
        return OC1_NOT_CONFIGURED;
    }
}
