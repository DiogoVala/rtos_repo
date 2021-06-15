/* 
 * File:   timer3.h
 * Author: Diogo Vala 
 *
 * Created on May 28, 2021, 12:08 PM
 */

#ifndef TIMER3_H
#define	TIMER3_H

#include <stdint.h>

/** \brief Function to configure the timer 
* @param [in] frequency The desired frequency (in Hz)
* @return Error code (-1 if failed, 0 success)
*/
int8_t Timer3Config(uint32_t frequency);


/** \brief Function to define the callback function to be used
* @param [in] (*function)(void) Pointer to the desired function
*/
void Timer3setCallbackFunction(void(*function)(void));


/** \brief Function to configure the timer to use interrupts or polling 
* @param [in] enable Enable the use of interrupts(0 for polling)
*/
void Timer3InterruptConfig(uint8_t interrupt_enable);

/** \brief Reads the flag signaling that a interrupt occured
* @return Interrupt flag
*/
uint8_t Timer3ReadInterruptFlag();


/** \brief Resets the interrupt flag 
*
*/
void Timer3ResetInterruptFlag();


/** \brief Starts the timer 
*
*/
void Timer3Start();


/** \brief Stops the timer 
*
*/
void Timer3Stop();
#endif	/* TIMER3_H */

