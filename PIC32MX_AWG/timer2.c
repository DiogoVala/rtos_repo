/* 
 * File:   timer.c
 * Author: Beatriz Silva & Diogo Vala
 *
 * Created on April 23, 2021, 11:19 AM
 */

#include "timer2.h"
#include <xc.h>
#include <sys/attribs.h>
#include <stdio.h>

#define PBCLOCK 40000000L
#define TIMER2_FREQUENCY_NOT_SUP -1
#define ERROR_F_NULL_REFERENCE -2

static void (*fp)(void);

int8_t Timer2Config(uint32_t frequency){
        
    static const uint16_t prescaler[] = {1, 2, 4, 8, 16, 32, 64, 256};
    static const uint8_t prescaler_size = sizeof prescaler;
	
    T2CONbits.TON = 0; // Stop timer
    IFS0bits.T2IF = 0; // Reset interrupt flag
    IPC2bits.T2IP = 4; // Interrupt Priority. Make sure it matches IPLx in ISR
    IEC0bits.T2IE = 0; // Enable T2 interrupts
    T2CONbits.T32 = 0; // 16 bit timer operation
    
    uint8_t i = 0;
    uint32_t pr2 = 0;
	while( i < prescaler_size ) {
        
        pr2 = (PBCLOCK/(frequency*prescaler[i]))-1;

		if( pr2 <= UINT16_MAX ) {
            PR2 = pr2;
			T2CONbits.TCKPS = i;// prescaler = 2^(n bits)
			TMR2 = 0;
			break;
		}
		i++;
        
        if (i == prescaler_size)
            return TIMER2_FREQUENCY_NOT_SUP;
	}
	return 0;
}


void Timer2InterruptConfig(uint8_t interrupt_enable) {
    if(interrupt_enable == 1){
        INTCONSET = _INTCON_MVEC_MASK;
        __builtin_enable_interrupts();
    }
    IEC0bits.T2IE = interrupt_enable; // Enable T2 interrupts
}


uint8_t Timer2ReadInterruptFlag(){
	return IFS0bits.T2IF;
}


void Timer2ResetInterruptFlag() {
	IFS0bits.T2IF = 0;	
}


void Timer2Start(){
	T2CONbits.TON = 1;	// Enable timer T2
}


void Timer2Stop(){
	T2CONbits.TON = 0;
}


void Timer2setCallbackFunction(void(*function)(void)){
    fp = function;
}


static void __ISR(_TIMER_2_VECTOR, IPL4AUTO) T2Interrupt(void){
    if (fp == NULL){
        fprintf(stderr,"Error: fp not set. Points to null");
        exit(ERROR_F_NULL_REFERENCE);
    }
    else{
       (*fp)(); 
    }
    Timer2ResetInterruptFlag();
}


