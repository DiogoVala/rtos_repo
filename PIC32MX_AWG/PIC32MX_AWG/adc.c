/* 
 * File:   adc.c
 * Author: Diogo Vala
 *
 * Overview: Configure ADC module
 */

#include <xc.h>
#include <stdlib.h>
#include "adc.h"

int8_t adcConfig(uint8_t nSamples, uint8_t AnalogPin){

    if (nSamples < 1 || nSamples > 16)
        return adcADC_INVALID_NUM_OF_SAMPLES;
    if (AnalogPin < 0 || AnalogPin > 15)
        return adcADC_INVALID_PIN;

    /* Disable JTAG interface as it uses a few ADC ports*/
    DDPCONbits.JTAGEN = 0; /* Debugging port that takes control of ADC IO pins*/

    /* Initialize ADC module*/
    AD1CON1bits.SSRC = 7; /* Clear SAMP bit ends sampling and starts conversion  */
    AD1CON1bits.CLRASAM = 1; /*Stop conversion when 1st A/D converter interrupt is generated and clears ASAM bit automatically*/
    AD1CON1bits.FORM = 0; /* Integer 16 bit output format*/
    AD1CON2bits.VCFG = 0; /* VR+=AVdd; VR-=AVss | Use internal voltage reference*/
    AD1CON2bits.SMPI = nSamples - 1; /* Number (-1) of consecutive conversions, stored in ADC1BUF0...ADCBUF{SMPI}*/
    AD1CON3bits.ADRC = 1; /* ADC Clock is derived from ADC Internal RC  */
    AD1CON3bits.SAMC = 16; /* Sample time is 16TAD ( TAD = 100ns)  (TAD = minimum ADC clock period)*/
    /* Set AN0 as input*/
    TRISBbits.TRISB0 = 1; // Set AN0 to input mode
    AD1CHSbits.CH0SA = AnalogPin; /* Select AN0 as input for A/D converter*/
    AD1PCFG = 0x01 << AnalogPin; /* Set AN0 to analog mode*/

    return adcADC_SUCCESS;
}

int8_t adcControl(uint8_t adcrun){
    if (adcrun == 1) {
        AD1CON1bits.ON = 1; /* Enable A/D module*/
    }
    else {
        AD1CON1bits.ON = 0; /* Enable A/D module*/
    }
}

uint16_t getADCsample(){    
    IFS1bits.AD1IF = 0; /* Reset interrupt flag*/
    AD1CON1bits.ASAM = 1; /* Start conversion*/
    while(IFS1bits.AD1IF == 0); /* Wait for EOC*/
    return ADC1BUF0;
}

/***************************************End Of File****************************/
