/*
 * 28-May-2021
 * Beatriz Silva 85276
 * Diogo Vala 64671
 *
 *******************************************************************************
 * This program runs 3 tasks.
 * IPC is achieved using Task Notify
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
#define mainTaskNotifyTASK_ACQ_PERIOD_MS 	( 100 / portTICK_RATE_MS )

/* Priorities of the demo application tasks (high numb. -> high prio.) */
#define mainTaskNotifyTASK_ACQ_PRIORITY     ( tskIDLE_PRIORITY + 3 )
#define mainTaskNotifyTASK_PROC_PRIORITY    ( tskIDLE_PRIORITY + 2 )
#define mainTaskNotifyTASK_OUT_PRIORITY	    ( tskIDLE_PRIORITY + 1 )

#define mainTaskNotifyADC_RESOLUTION 10
#define mainTaskNotifyADC_PIN 0 /* What Analog pin to use */
#define mainTaskNotifyADC_RUN 1 /* 0- Disable ADC ; 1- Enable ADC */
#define mainTaskNotifyADC_NUM_SAMPLES 1 /* Number of samples taken by ADC at one time */
#define mainTaskNotifyMAX_TEMP 100 /* Maximum temperature */
#define mainTaskNotifyPROC_NUM_SAMPLES 5 /* Number of samples to average */
#define mainTaskNotifyOUT_STRING_MAX_SIZE 10 /*Maximum string size to print result*/

static volatile uint16_t usADCSample=0; /*Sample taken by ADC & converted to 0-100*/
static volatile uint16_t usADCAverage=0; /* Average of N samples */
static volatile uint8_t usADCAverage_FP=0; /* Floating point component of average */

/* Task handles */
static TaskHandle_t xProc = NULL, xOut = NULL;

/*
 * Prototypes and tasks
 */

void pvAcq(void *pvParam)
{
    TickType_t xLastWakeTime;
    
    xLastWakeTime = xTaskGetTickCount();

    while(1) {
        
        usADCSample=((getADCsample()+1)*mainTaskNotifyMAX_TEMP)>>mainTaskNotifyADC_RESOLUTION;
        xTaskNotifyGive(xProc);
        vTaskDelayUntil(&xLastWakeTime, mainTaskNotifyTASK_ACQ_PERIOD_MS);  
    }
}

void pvProc(void *pvParam)
{
    static uint8_t usSample_count=0;
    
    while(1) 
    {
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        usADCAverage+=usADCSample;
        usSample_count++;
        if(usSample_count==mainTaskNotifyPROC_NUM_SAMPLES){
            usADCAverage_FP=usADCAverage%mainTaskNotifyPROC_NUM_SAMPLES;
            usADCAverage/=mainTaskNotifyPROC_NUM_SAMPLES;
            usSample_count=0;   
            xTaskNotifyGive(xOut);
        }
    }
}

void pvOut(void *pvParam)
{
    static uint8_t ucStr[mainTaskNotifyOUT_STRING_MAX_SIZE];
    
    while(1) {
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        snprintf(ucStr,mainTaskNotifyOUT_STRING_MAX_SIZE,"\r\n%d.%1d", usADCAverage, usADCAverage_FP);
        PrintStr(ucStr);
        usADCAverage=0;
    }
}

/*
 * Create the demo tasks then start the scheduler.
 */
int mainTaskNotify( void )
{
    TRISBbits.TRISB0 = 1; // Set AN0 to input mode
    adcConfig(mainTaskNotifyADC_NUM_SAMPLES, mainTaskNotifyADC_PIN);
    adcControl(mainTaskNotifyADC_RUN);

    
	// Init UART and redirect tdin/stdot/stderr to UART
    if(UartInit(configPERIPHERAL_CLOCK_HZ, 115200) != UART_SUCCESS) {
        PORTAbits.RA3 = 1; // If Led active error initializing UART
        while(1);
    }

     __XC_UART = 1; /* Redirect stdin/stdout/stderr to UART1*/
          
    /* Create the tasks defined within this file. */
	xTaskCreate( pvAcq, ( const signed char * const )  "Acq",  configMINIMAL_STACK_SIZE, NULL, mainTaskNotifyTASK_ACQ_PRIORITY, NULL );
    xTaskCreate( pvProc, ( const signed char * const ) "Proc", configMINIMAL_STACK_SIZE, NULL, mainTaskNotifyTASK_PROC_PRIORITY, &xProc );
    xTaskCreate( pvOut, ( const signed char * const )  "Out",  configMINIMAL_STACK_SIZE, NULL, mainTaskNotifyTASK_OUT_PRIORITY, &xOut );
 
    /* Finally start the scheduler. */
	vTaskStartScheduler();

	/* Will only reach here if there is insufficient heap available to start
	the scheduler. */
	return 0;
}