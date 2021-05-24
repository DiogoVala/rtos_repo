/*
 * 28-May-2021
 * Beatriz Silva 85276
 * Diogo Vala 64671
 *
 *******************************************************************************
 * This program runs 3 tasks.
 * IPC is achieved using Semaphores.
 * Task ACQ takes samples from AN0. 
 * Task PROC takes each sample and sums them together. At 5 samples,
 * calculates average.
 * Task OUT takes the average and prints the result.
 *  
 */

/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/* App includes */
#include "../UART/uart.h"

/* The rate at which the data is sampled */
#define mainSemphrDATA_ACQ_PERIOD_MS 	( 100 / portTICK_RATE_MS )

/* Priorities of the demo application tasks (high numb. -> high prio.) */
#define mainSemphrTASK_ACQ_PRIORITY     ( tskIDLE_PRIORITY + 3 )
#define mainSemphrTASK_PROC_PRIORITY    ( tskIDLE_PRIORITY + 2 )
#define mainSemphrTASK_OUT_PRIORITY     ( tskIDLE_PRIORITY + 1 )

#define mainSemphrADC_RESOLUTION 10 /* ADC 10 bit resolution*/
#define mainSemphrADC_PIN 0 /* What Analog pin to use */
#define mainSemphrADC_RUN 1 /* 0- Disable ADC ; 1- Enable ADC */
#define mainSemphrADC_NUM_SAMPLES 1 /* Number of samples taken by ADC at one time */

#define mainSemphrMAX_TEMP 100 /* Maximum temperature */
#define mainSemphrPROC_NUM_SAMPLES 5 /* Number of samples to average */
#define mainSemphrOUT_STRING_MAX_SIZE 10 /*Maximum string size to print result*/

SemaphoreHandle_t xSemaphore_Proc = NULL;
SemaphoreHandle_t xSemaphore_Out = NULL;

static volatile uint16_t usADCSample=0; /*Sample taken by ADC & converted to 0-100*/
static volatile uint16_t usADCAverage=0; /* Average of N samples */
static volatile uint8_t ucADCAverage_FP=0; /* Floating point component of average */

/*
 * Prototypes and tasks
 */

void pvAcq(void *pvParam)
{
    static TickType_t xLastWakeTime = 0;
    xSemaphore_Proc = xSemaphoreCreateBinary();
    
    xLastWakeTime = xTaskGetTickCount();
    
    while(1) {
        
        usADCSample=((getADCsample()+1)*mainSemphrMAX_TEMP)>>mainSemphrADC_RESOLUTION;
        xSemaphoreGive(xSemaphore_Proc);
        vTaskDelayUntil(&xLastWakeTime, mainSemphrDATA_ACQ_PERIOD_MS);  
    }
}

void pvProc(void *pvParam)
{
    static uint8_t ucSample_count=0;
    xSemaphore_Out = xSemaphoreCreateBinary();
    
    while(1) {
        if(xSemaphore_Proc != NULL) {
            if (xSemaphoreTake(xSemaphore_Proc, portMAX_DELAY) == pdTRUE){ /* Blocked forever until SemaphoreGive*/
                usADCAverage+=usADCSample;
                ucSample_count++;
                if(ucSample_count==mainSemphrPROC_NUM_SAMPLES){
                    ucADCAverage_FP=usADCAverage%mainSemphrPROC_NUM_SAMPLES;
                    usADCAverage/=mainSemphrPROC_NUM_SAMPLES;
                    ucSample_count=0;
                    xSemaphoreGive(xSemaphore_Out);
                }
            }
        }
    }
}

void pvOut(void *pvParam)
{
    static uint8_t ucStr[mainSemphrOUT_STRING_MAX_SIZE];
    
    while(1) {
        if(xSemaphore_Out != NULL) {
            if (xSemaphoreTake(xSemaphore_Out, portMAX_DELAY) == pdTRUE){
                snprintf(ucStr,mainSemphrOUT_STRING_MAX_SIZE,"\r\n%d.%1d", usADCAverage, ucADCAverage_FP);
                PrintStr(ucStr);
                usADCAverage=0;
            }
        }
    }
}

/*
 * Create the demo tasks then start the scheduler.
 */
int mainSemphr( void )
{
    TRISBbits.TRISB0 = 1; // Set AN0 to input mode
    adcConfig(mainSemphrADC_NUM_SAMPLES, mainSemphrADC_PIN);
    adcControl(mainSemphrADC_RUN); /* Start ADC module */

    
	// Init UART and redirect tdin/stdot/stderr to UART
    if(UartInit(configPERIPHERAL_CLOCK_HZ, 115200) != UART_SUCCESS) {
        PORTAbits.RA3 = 1; // If Led active error initializing UART
        while(1);
    }

     __XC_UART = 1; /* Redirect stdin/stdout/stderr to UART1*/
          
    /* Create the tasks defined within this file. */
	xTaskCreate( pvAcq, ( const signed char * const )  "Acq",  configMINIMAL_STACK_SIZE, NULL, mainSemphrTASK_ACQ_PRIORITY, NULL );
    xTaskCreate( pvProc, ( const signed char * const ) "Proc", configMINIMAL_STACK_SIZE, NULL, mainSemphrTASK_PROC_PRIORITY, NULL );
    xTaskCreate( pvOut, ( const signed char * const )  "Out",  configMINIMAL_STACK_SIZE, NULL, mainSemphrTASK_OUT_PRIORITY, NULL );
 
    /* Finally start the scheduler. */
	vTaskStartScheduler();

	/* Will only reach here if there is insufficient heap available to start
	the scheduler. */
	return 0;
}