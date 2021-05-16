/* 
 * File:   uart.c
 * Author: Paulo Pedreiras
 *
 * Created on Jan 28, 2019
 * MPLAB X IDE v5.10 + XC32 v2.15
 *
 * Target: Digilent chipKIT MAx32 board 
 * 
 * Overview:
 *          Set of functions to handle the UART       
 
 * Notes: Partially based on the bootloader code from Microchip
 * 
 * Revisions:
 *      2017-10-25: initial release
 *      2019-01-28: updated to MPLAB X IDE v5.\0 + XC32 v2.15
 */


#include <xc.h>
#include <stdlib.h>
#include <stdint.h>
#include "uart.h"

/********************************************************************
* Function: 	UartInit()
* Precondition: 
* Input: 		PB Clock and baudrate
* Returns:      UART_SUCCESS if Ok.
*               UARTX_XXX error codes in case of failure (see uart.h)
* Side Effects:	Takes control of U1A TX and RX pins
* Overview:     Initializes UART.
*		
* Note:		 	Only supports 40MHz PBCLOCK, UART1A and 
*               {9600/115200},8,n,1 configuration
* 
********************************************************************/	
int UartInit(uint64_t pbclock, uint32_t br)
{
   if(pbclock != 40000000L) 
       return UART_PBCLOCK_NOT_SUP; // Todo: add support to common pbclock values.
       
   // In the following are used the table values provided in the datasheet
   //PIC32MX Family Reference Manual, DS61107E-pages 21-14 and following
 	switch(br) {
        case 115200:
            U1ABRG=21; 
            U1AMODEbits.BRGH = 0;
            break;
            
        case 9600:
            U1ABRG=259; 
            U1AMODEbits.BRGH = 0;
            break;
        default:
            return UART_BR_NOT_SUP; // Baudrate not supported
    }
    
    // Common configuration settings
    U1AMODEbits.SIDL=0; // Continue operation in idle mode 
    U1AMODEbits.IREN=0; //Disable Irda
    U1AMODEbits.UEN=0;  //Only use TX and RX pints. No HW flow control 
    U1AMODEbits.WAKE=0;  //Wake -up on start bit disabled
    U1AMODEbits.LPBACK=0;  //No loopback  
    U1AMODEbits.ABAUD=0;  //No autobaud
    U1AMODEbits.RXINV=0;  //Idle logic value is 1
    U1AMODEbits.PDSEL=0;  //8 bit data, no parity
    U1AMODEbits.STSEL=0;  //1 stop bit
    U1STAbits.ADM_EN = 0; // No automatic address detection
    U1STAbits.UTXISEL = 0; // Interrupt when TX buffer has at least 1 empty position
    U1STAbits.UTXINV = 0; // Idle logic value is 1
    
    // Configuration done. Enable.   
    U1AMODEbits.ON = 1;  
    U1STAbits.UTXEN = 1;
    U1STAbits.URXEN = 1;
    
    return UART_SUCCESS;
    
}	


/********************************************************************
* Function: 	UartClose()
* Precondition: 
* Input: 		None
* Output:		None.
* Side Effects:	None.
* Overview:     Closes UART connection.
* Note:		 	No function currently
********************************************************************/	
int UartClose(void)
{
	//TODO: do some closing operation if required.	
    return UART_SUCCESS;
}	


/********************************************************************
* Function: 	GetChar()
* Precondition: UART initialized
* Input: 		None
* Output:		UART_SUCESS: If there is some data
 *              UART_FAIL: if there is no data.
* Side Effects:	None.
* Overview:     Gets the data from UART RX FIFO.
* Note:		 	None.
********************************************************************/
int GetChar(uint8_t *byte)
{
	char dummy;

	if(U1STAbits.OERR ||U1STAbits.FERR || U1STAbits.PERR) // receive errors?
	{
		dummy = U1RXREG; 			// dummy read to clear FERR/PERR
		U1STAbits.OERR = 0;			// clear OERR to keep receiving
	}

	if(U1STAbits.URXDA)
	{
		*byte = U1ARXREG;		        // get data from UART RX FIFO
		return UART_SUCCESS;
	}
	
	return UART_FAIL;

}


/********************************************************************
* Function: 	PutChar()
* Precondition: UART Initialized
* Input: 		Character
* Output:		None
* Side Effects:	None.
* Overview:     Puts the data into UART tx reg for transmission.
* Note:		 	None.
********************************************************************/
void PutChar(uint8_t txChar)
{
    while(U1STAbits.UTXBF); // wait for TX buffer to be empty
    U1ATXREG = txChar;
}

/********************************************************************
* Function: 	PrintStr()
* Precondition: UART Initialized
* Input: 		NULL terminated string 
* Output:		None
* Side Effects:	None.
* Overview:     Prints the input string.
* Note:		 	None.
********************************************************************/
void PrintStr(uint8_t *txStr)
{
    while(*txStr != NULL)
        PutChar(*(txStr++));
}

/***************************************End Of File*************************************/


