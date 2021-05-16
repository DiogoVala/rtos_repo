/*
 * 28-May-2021
 * Beatriz Silva 85276
 * Diogo Vala 64671
 *
 *******************************************************************************
 * This program runs 3 tasks.
 * IPC is achieved using Queues.
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
#define mainQueueTASK_ACQ_PERIOD_MS 	( 100 / portTICK_RATE_MS )

/* Priorities of the demo application tasks (high numb. -> high prio.) */
#define mainQueueTASK_ACQ_PRIORITY      ( tskIDLE_PRIORITY + 3 )
#define mainQueueTASK_PROC_PRIORITY	    ( tskIDLE_PRIORITY + 2 )
#define mainQueueTASK_OUT_PRIORITY	    ( tskIDLE_PRIORITY + 1 )

#define mainQueueADC_RESOLUTION 10
#define mainQueueADC_PIN 0
#define mainQueueADC_RUN 1
#define mainQueueADC_NUM_SAMPLES 1
#define mainQueueMAX_TEMP 100
#define mainQueuePROC_NUM_SAMPLES 5
#define mainQueueOUT_STRING_MAX_SIZE 10

#define mainQueueQUEUE_SAMPLES_SIZE 5
#define mainQueueQUEUE_AVERAGES_SIZE 1

QueueHandle_t xSamplesQueue = NULL;
QueueHandle_t xAveragesQueue = NULL;

typedef struct {
  uint16_t usADCAverage; /* Average of N samples */
  uint16_t usADCAverage_FP; /* Floating point component of average */
} xResult_t;

/*
 * Prototypes and tasks
 */

void pvAcq(void *pvParam)
{
    static TickType_t xLastWakeTime = 0;
    static uint16_t usADCSample=0;
    portBASE_TYPE xStatus;
    
    xSamplesQueue = xQueueCreate(mainQueueQUEUE_SAMPLES_SIZE, sizeof(usADCSample));
    
    while(1) {
        xLastWakeTime = xTaskGetTickCount();
        usADCSample =((getADCsample()+1)*mainQueueMAX_TEMP)>>mainQueueADC_RESOLUTION;
        xStatus = xQueueSend(xSamplesQueue, (void *)&usADCSample, (TickType_t)0);
        if(xStatus != pdPASS){
            PrintStr("\r\nError: Could not send value to Samples queue.");
        }
        vTaskDelayUntil(&xLastWakeTime, mainQueueTASK_ACQ_PERIOD_MS);  
    }
}

void pvProc(void *pvParam)
{
    static uint8_t ucSample_count=0;
    static uint16_t usRxedADCSample=0;
    static xResult_t xResult;
    portBASE_TYPE xStatus;
    xAveragesQueue = xQueueCreate(mainQueueQUEUE_AVERAGES_SIZE, sizeof(xResult));
    
    while(1) {
        if(xSamplesQueue != NULL) {
            if (xQueueReceive(xSamplesQueue, (uint16_t*)&usRxedADCSample, portMAX_DELAY) == pdPASS ) {
                xResult.usADCAverage += usRxedADCSample;
                ucSample_count++;
                
                if(ucSample_count == mainQueuePROC_NUM_SAMPLES){
                    xResult.usADCAverage_FP = xResult.usADCAverage%mainQueuePROC_NUM_SAMPLES;
                    xResult.usADCAverage /= mainQueuePROC_NUM_SAMPLES;
                    ucSample_count=0;
                    
                    xStatus = xQueueSend(xAveragesQueue, (void *)&xResult, (TickType_t)0);
                    if(xStatus != pdPASS){
                        PrintStr("\r\nError: Could not send value to Averages queue.");
                    }
                    xResult.usADCAverage=0;
                }
            }
        }
    }
}

void pvOut(void *pvParam)
{
    static xResult_t xRxedResult;
    static uint8_t ucStr[mainQueueOUT_STRING_MAX_SIZE];
   
    while(1) {
        if(xAveragesQueue != NULL) {
            if (xQueueReceive(xAveragesQueue, (xResult_t*)&xRxedResult, portMAX_DELAY) == pdPASS ) {
                snprintf(ucStr,mainQueueOUT_STRING_MAX_SIZE,"\r\nTemperature: %d.%d", xRxedResult.usADCAverage, xRxedResult.usADCAverage_FP);
                PrintStr(ucStr);
            }
        }
    }
}

/*
 * Create the demo tasks then start the scheduler.
 */
int mainQueue( void )
{
    TRISBbits.TRISB0 = 1; // Set AN0 to input mode
    adcConfig(mainQueueADC_NUM_SAMPLES, mainQueueADC_PIN);
    adcControl(mainQueueADC_RUN);

    
	// Init UART and redirect tdin/stdot/stderr to UART
    if(UartInit(configPERIPHERAL_CLOCK_HZ, 115200) != UART_SUCCESS) {
        PORTAbits.RA3 = 1; // If Led active error initializing UART
        while(1);
    }

     __XC_UART = 1; /* Redirect stdin/stdout/stderr to UART1*/
          
    /* Create the tasks defined within this file. */
	xTaskCreate( pvAcq, ( const signed char * const ) "Acq", configMINIMAL_STACK_SIZE, NULL, mainQueueTASK_ACQ_PRIORITY, NULL );
    xTaskCreate( pvProc, ( const signed char * const ) "Proc", configMINIMAL_STACK_SIZE, NULL, mainQueueTASK_PROC_PRIORITY, NULL );
    xTaskCreate( pvOut, ( const signed char * const ) "Out", configMINIMAL_STACK_SIZE, NULL, mainQueueTASK_OUT_PRIORITY, NULL );

    /* Finally start the scheduler. */
	vTaskStartScheduler();

	/* Will only reach here if there is insufficient heap available to start
	the scheduler. */
	return 0;
}