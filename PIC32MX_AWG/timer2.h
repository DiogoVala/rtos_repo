#ifndef TIMER2_H
#define TIMER2_H

#include <stdint.h>

/** \brief Function to configure the timer 
* @param [in] frequency The desired frequency (in Hz)
* @return Error code (-1 if failed, 0 success)
*/
int8_t Timer2Config(uint32_t frequency);


/** \brief Function to define the callback function to be used
* @param [in] (*function)(void) Pointer to the desired function
*/
void Timer2setCallbackFunction(void(*function)(void));


/** \brief Function to configure the timer to use interrupts or polling 
* @param [in] enable Enable the use of interrupts(0 for polling)
*/
void Timer2InterruptConfig(uint8_t interrupt_enable);

/** \brief Reads the flag signaling that a interrupt occured
* @return Interrupt flag
*/
uint8_t Timer2ReadInterruptFlag();


/** \brief Resets the interrupt flag 
*
*/
void Timer2ResetInterruptFlag();


/** \brief Starts the timer 
*
*/
void Timer2Start();


/** \brief Stops the timer 
*
*/
void Timer2Stop();

#endif