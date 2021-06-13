/* 
 * File:   timer3.c
 * Author: Beatriz Silva & Diogo Vala
 *
 * Created on April 23, 2021, 11:19 AM
 */

#include "timer3.h"
#include <xc.h>
#include <sys/attribs.h>
#include <stdio.h>

#define PBCLOCK 40000000L
#define TIMER3_FREQUENCY_NOT_SUP -1
#define ERROR_F_NULL_REFERENCE -2

static void (*fp)(void);

int8_t Timer3Config(uint32_t frequency){
        
    static const uint16_t prescaler[] = {1, 2, 4, 8, 16, 32, 64, 256};
    static const uint8_t prescaler_size = sizeof prescaler;
	
    T3CONbits.TON = 0; // Stop timer
    IFS0bits.T3IF = 0; // Reset interrupt flag
    IPC3bits.T3IP = 4; // Interrupt Priority. Make sure it matches IPLx in ISR
    IEC0bits.T3IE = 0; // Enable T2 interrupts
    
    uint8_t i = 0;
    uint32_t pr3 = 0;
    if(frequency!=0){
        while( i < prescaler_size ) {

            pr3 = (PBCLOCK/(frequency*prescaler[i]))-1;

            if( pr3 <= UINT16_MAX ) {
                PR3 = pr3;
                T3CONbits.TCKPS = i;// prescaler = 2^(n bits)
                TMR3 = 0;
                break;
            }
            i++;

            if (i == prescaler_size)
                return TIMER3_FREQUENCY_NOT_SUP;
        }
    }
    else
    {
        return TIMER3_FREQUENCY_NOT_SUP;
    }
	return 0;
}


void Timer3InterruptConfig(uint8_t interrupt_enable) {
    IEC0bits.T3IE = interrupt_enable; // Enable T2 interrupts
}


uint8_t Timer3ReadInterruptFlag(){
	return IFS0bits.T3IF;
}


void Timer3ResetInterruptFlag() {
	IFS0bits.T3IF = 0;	
}


void Timer3Start(){
	T3CONbits.TON = 1;	// Enable timer T2
}


void Timer3Stop(){
	T3CONbits.TON = 0;
}

