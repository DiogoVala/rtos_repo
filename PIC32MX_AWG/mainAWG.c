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
#include <math.h>
#include <stdbool.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <sys/attribs.h>

/* App includes */
#include "../UART/uart.h"
#include "timer2.h"
#include "timer3.h"
#include "oc1.h"

/* Priorities of the demo application tasks (high numb. -> high prio.) */
#define mainAWGTASK_LOADWAVEFORM_PRIORITY           ( tskIDLE_PRIORITY + 3 )
#define mainAWGTASK_INTERFACE_PRIORITY              ( tskIDLE_PRIORITY + 2 )
#define mainAWGTASK_WAVEFORM_GENERATOR_PRIORITY	    ( tskIDLE_PRIORITY + 1 )

#define mainAwgPWM_FREQUENCY 200000 /* frequency of PWM signal to be applied to filter*/
#define mainAwgSELECTED_TIMER 2 /* Timer 2 as source for OC1 */

#define mainAwgWAVEFORM_SIZE 400 /* Number of duty cycle samples per period of output signal*/

/* Max vars of system */
#define mainAwgMAX_FREQUENCY 300
#define mainAwgMAX_AMPLITUDE 33
#define mainAwgMAX_PHASE 360

#define mainAwgINPUT_BUFFER_SIZE 6 /* Max number of bytes to add to input buffer */

static const uint16_t mainAwgTRIGGER_WIDTH = mainAwgWAVEFORM_SIZE/20; /* Number of samples that the trigger stays HIGH*/

static volatile uint8_t ucWavetype; /* 0: sine; 1: square; 2: sawtooth*/
static volatile uint16_t usFrequency = 0; /* Desired output signal frequency*/
static volatile uint8_t ucWaveAmplitude = 0; /* Desired output signal Amplitude 0-33 (0-3.3V) */
static volatile uint16_t usPhase = 0; /* Desired output signal phase*/

static volatile uint16_t usWaveform[mainAwgWAVEFORM_SIZE] = {0}; /* Array to store waveform duty cycle samples */
static volatile uint16_t usArbitraryWaveform[mainAwgWAVEFORM_SIZE] = {0}; /* Array to store arbitrary waveform */

static volatile uint16_t usWaveIndex = 0; /* Index of usWaveform[] to send to OC1 */
static volatile uint16_t usMaxDutycycle = 0; /* Maximum value of duty cycle according to desired amplitude */

static volatile bool ucCommandOrWave = true; /* To distinguish between normal command or waveform file input*/

QueueHandle_t xInputQueue = NULL;
QueueHandle_t xArbWaveInputQueue = NULL;

/* Wave types */
enum WaveForm_t{
    WAVE_OFF,
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_ARBITRARY
};

/* Task handles */
static TaskHandle_t xSequencer = NULL, xInterface = NULL, xWaveformGenerator= NULL, xLoadWave = NULL;

/*
 * Prototypes and tasks
 */

/* Task called when system is receiving an arbitrary waveform file 
 * 
 * Takes each byte from UART and forms integer samples to be stored
 * in usArbitraryWaveform[].
 */
void pvLoadWaveform( void *pvParam)
{
    static uint16_t usIterator = 0;
    uint8_t ucInput = '\0';
    uint16_t usNewVal = 0;
            
    while(1)
    {
        xQueueReceive( xArbWaveInputQueue, (uint8_t*)&ucInput, portMAX_DELAY);
        
        Timer3Stop();
        ucCommandOrWave = false;
        
        /* Form sample */
        if(ucInput!='\r' && ucInput!='\n'){
            usNewVal=usNewVal*10+(ucInput-'0');
        }
        else /* Save current value and go to next value */
        {
            printf("\r%d, %d \n", usIterator, usNewVal);
            usArbitraryWaveform[usIterator]=usNewVal;
            usNewVal=0;
            
            usIterator++;
            /* Ignore any further inputs when array is full */
            if(usIterator == mainAwgWAVEFORM_SIZE)
            {
                xQueueReset(xArbWaveInputQueue);
                ucCommandOrWave=true; /* Switch UART target back to the Interface Task */
                usIterator=0;
            }
        }
    }
}

/* Task called when a new byte is received from UART
 * 
 * Evaluates each byte and constructs a command sequence.
 * Updates system variables according to the command sequence.
 */
void pvInterface(void *pvParam)
{    
    static uint8_t ucBuffer[mainAwgINPUT_BUFFER_SIZE]={'0'};
    static uint8_t ucBufIdx=0;
        
    uint8_t  ucInput = '\0';
    uint8_t  ucCommand = '\0';
    
    uint16_t usNewFrequency;
    uint8_t  ucNewWaveAmplitude;
    uint16_t usNewPhase;
    
    while(1) {
        xQueueReceive( xInputQueue, (uint8_t*)&ucInput, portMAX_DELAY);
        
       
        /* Reset input buffer and current queue*/
        if(ucInput == 'r')
        {
            printf("\r\n");
            ucBufIdx=0;
            xQueueReset(xArbWaveInputQueue);
            memset(ucBuffer, '\0', mainAwgINPUT_BUFFER_SIZE);
        }
        /* Put valid bytes into input buffer */
        else if((ucInput >= '0'  && ucInput <= '9' ) || (ucInput >= 'A'  && ucInput <= 'Z' )\
                || (ucInput >= 'a'  && ucInput <= 'z' )){
            
            printf("%c", ucInput);
            ucBuffer[ucBufIdx]=ucInput;
            ucBufIdx++;
            if(ucBufIdx>=mainAwgINPUT_BUFFER_SIZE-1){
                ucBufIdx=0;
                memset(ucBuffer, '\0', mainAwgINPUT_BUFFER_SIZE);
                printf("\rInvalid command.\n");
            }  
        }
        /* On New line, evaluate buffer and update variables */
        else if(ucInput=='\r')
        {
            printf("\r\n\n");
            
            Timer3Stop(); /* Stop wave output */
            
            ucCommand=ucBuffer[0];
            
            switch(ucCommand){
                case 'f':
                case 'F':
                    usNewFrequency=(ucBuffer[1]-'0')*100+(ucBuffer[2]-'0')*10+(ucBuffer[3]-'0');
                    break;
                case 'v':
                case 'V':
                    ucNewWaveAmplitude=(ucBuffer[1]-'0')*10+(ucBuffer[2]-'0');
                    break;
                case 'p':
                case 'P':
                    usNewPhase = (ucBuffer[1]-'0')*100+(ucBuffer[2]-'0')*10+(ucBuffer[3]-'0');
                    break;
                case 's':
                case 'S':
                    ucWavetype=WAVE_SINE;
                    printf("\rWave: Sine\n");
                    break;
                case 't':
                case'T':
                    ucWavetype=WAVE_TRIANGLE;
                    printf("\rWave: Triangle\n");
                    break; 
                case 'q':
                case 'Q':
                    ucWavetype=WAVE_SQUARE;
                    printf("\rWave: Square\n");
                    break;
                case 'a':
                case 'A':
                    ucWavetype=WAVE_ARBITRARY;
                    printf("\rWave: Arbitrary\n");
                    break;
                case 'o':
                case 'O':
                    ucWavetype=WAVE_OFF;
                    printf("\rWave: Off\n");
                    break;
                default:
                    printf("\rInvalid Command.\n");
                    break;
            }
            /* Empty input buffer */
            ucBufIdx=0;
            memset(ucBuffer, '\0', mainAwgINPUT_BUFFER_SIZE);
           
            if(usNewFrequency<=mainAwgMAX_FREQUENCY)
            {
                usFrequency=usNewFrequency;
                printf("\rFreq: %d Hz\n", usFrequency);
            }
            else
            {
                printf("\rInvalid frequency.\n");
            }

            if(ucNewWaveAmplitude<=mainAwgMAX_AMPLITUDE)
            {
                ucWaveAmplitude = ucNewWaveAmplitude;
                usMaxDutycycle = ucWaveAmplitude*oc1MAX_DUTYCYCLE/mainAwgMAX_AMPLITUDE;
                printf("\rAmp: %d.%d V\n", ucWaveAmplitude/10,ucWaveAmplitude%10);
            }
            else
            {
                printf("\rInvalid amplitude.\n");
            }
            
            if(usNewPhase<=mainAwgMAX_PHASE)
            {
                usPhase = usNewPhase;
                usWaveIndex = usPhase*mainAwgWAVEFORM_SIZE/mainAwgMAX_PHASE;
                printf("\rPhase: %d Deg\n\n", usPhase);
            }
            else
            {
                printf("\rInvalid phase.\n");
            }
            
            xTaskNotifyGive(xWaveformGenerator);
        }
    }
}

/* Task called to generate usWaveform[] array of samples for desired output signal 
 * 
 * Generates one period of the signal with the desired amplitude
 */
void pvWaveformGenerator(void *pvParam)
{
    uint16_t usIterator;
    
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        switch(ucWavetype){
            case WAVE_SINE:
                for(usIterator = 0; usIterator < mainAwgWAVEFORM_SIZE; usIterator++)
                {
                    usWaveform[usIterator]=(usMaxDutycycle/2*sin((2*M_PI*(usIterator+1))/mainAwgWAVEFORM_SIZE)+usMaxDutycycle/2);
                }
                break;
                
            case WAVE_SQUARE:
                for(usIterator = 0; usIterator < mainAwgWAVEFORM_SIZE; usIterator++)
                {
                    if(usIterator<(mainAwgWAVEFORM_SIZE/2))
                    {
                        usWaveform[usIterator]=0;
                    }
                    else
                    {
                        usWaveform[usIterator]=usMaxDutycycle;
                    }
                }
                break;
                
            case WAVE_TRIANGLE:
                for(usIterator = 0; usIterator < mainAwgWAVEFORM_SIZE; usIterator++)
                {
                    if(usIterator<(mainAwgWAVEFORM_SIZE/2))
                    {
                        usWaveform[usIterator]=2*(uint16_t)((usIterator*(float)usMaxDutycycle/(float)mainAwgWAVEFORM_SIZE));
                    }
                    else
                    {
                        usWaveform[usIterator]=2*(uint16_t)(usMaxDutycycle-(usIterator*(float)usMaxDutycycle/(float)mainAwgWAVEFORM_SIZE));
                    }
                }
                break;
            case WAVE_ARBITRARY:
                for(usIterator = 0; usIterator < mainAwgWAVEFORM_SIZE; usIterator++)
                {
                    usWaveform[usIterator]=(uint16_t)(usArbitraryWaveform[usIterator]*ucWaveAmplitude/mainAwgMAX_AMPLITUDE);
                }
                break;
            case WAVE_OFF:
                for(usIterator = 0; usIterator < mainAwgWAVEFORM_SIZE; usIterator++)
                {
                    usWaveform[usIterator]=0;
                }
                break;
        }
        
        /* Update Timer 3 */
        Timer3Config(mainAwgWAVEFORM_SIZE*usFrequency);
        Timer3InterruptConfig(1);
        Timer3Start();
    }
}

/*
 * Create the demo tasks then start the scheduler.
 */
int mainAWG( void )
{
    /* ISRs */
    void __attribute__( (interrupt(IPL2AUTO), vector(_TIMER_3_VECTOR))) vT3InterruptWrapper(void);
    void __attribute__( (interrupt(IPL5AUTO), vector(_UART_1_VECTOR))) vU1InterruptWrapper(void);
    
    /* PWM Timer and OC */
    Timer2Config(mainAwgPWM_FREQUENCY);
    OC1Config(mainAwgSELECTED_TIMER);
    OC1Control(oc1START);
    Timer2Start();
    OC1SetDutyCycle(0);
     
    // Set RD0 as digital output (OC1)
    TRISDbits.TRISD0 = 0;
    PORTDbits.RD0 = 1;
    
    // Set RE8 as digital output (External trigger)
    TRISEbits.TRISE8 = 0;
    PORTEbits.RE8 = 0;
    
    
	// Init UART and redirect tdin/stdot/stderr to UART
    if(UartInit(configPERIPHERAL_CLOCK_HZ, 115200) != UART_SUCCESS) {
        PORTAbits.RA3 = 1; // If Led active error initializing UART
        while(1);
    }
    
    
    U1STAbits.URXISEL = 0; /* Interrupt on each new byte */
    IPC6bits.U1IP = 5; /* Interrupt priority */
    IEC0bits.U1RXIE = 1; /* Enable receive interupts */
    IFS0bits.U1RXIF = 0; /* Clear the rx interrupt flag */
    __XC_UART = 1; /* Redirect stdin/stdout/stderr to UART1 */
    
    printf("\033[2J");
    printf("\rArbitrary Waveform Generator - Diogo Vala & Beatriz Silva\n");
    printf("\rCommands: \n");
    printf("\rs (sine); t(triangle); q(square); a(arbitrary)\n");
    printf("\rFxxx -> 000-300\n");
    printf("\rVxx -> 00-33\n");
    printf("\rPxxx -> 000-360\n");
    
    xInputQueue = xQueueCreate(mainAwgINPUT_BUFFER_SIZE, sizeof(uint8_t));
    xArbWaveInputQueue = xQueueCreate(mainAwgWAVEFORM_SIZE*10, sizeof(uint8_t));
    
    /* Create the tasks defined within this file. */
    xTaskCreate( pvLoadWaveform, ( const signed char * const ) "LoadWave", configMINIMAL_STACK_SIZE, NULL, mainAWGTASK_LOADWAVEFORM_PRIORITY, &xLoadWave );
    xTaskCreate( pvInterface, ( const signed char * const ) "Interface", configMINIMAL_STACK_SIZE, NULL, mainAWGTASK_INTERFACE_PRIORITY, &xInterface );
    xTaskCreate( pvWaveformGenerator, ( const signed char * const ) "WaveformGenerator", configMINIMAL_STACK_SIZE, NULL, mainAWGTASK_WAVEFORM_GENERATOR_PRIORITY, &xWaveformGenerator );

    /* Finally start the scheduler. */
	vTaskStartScheduler();   

	/* Will only reach here if there is insufficient heap available to start
	the scheduler. */
	return 0;
}

/* Timer3 ISR */
void vT3InterruptHandler(void)
{
    OC1SetDutyCycle(usWaveform[usWaveIndex]);
    
    /* External trigger */
    if(usWaveIndex < mainAwgTRIGGER_WIDTH)
    {
        LATEbits.LATE8 = 1;
    }
    else
    {
        LATEbits.LATE8 = 0;
    }
    
    usWaveIndex++;
    if(usWaveIndex>=mainAwgWAVEFORM_SIZE)
    {
        usWaveIndex=0;
    }
    
    Timer3ResetInterruptFlag();
}

/* UART ISR */
void vU1InterruptHandler(void) {
    
    BaseType_t xHigherPriorityTaskWoken = pdTRUE;
    
    uint8_t ucInput=0;
    
    /*Todo: Error checking*/
    while (!U1STAbits.URXDA);
    if ((U1STAbits.PERR == 0) && (U1STAbits.FERR == 0)) {
        GetChar(&ucInput);
    }
    
    IFS0bits.U1RXIF = 0; //clear the rx interupt flag  
    
    if(ucInput!='l' && ucCommandOrWave)
    {
        xQueueSendFromISR(xInputQueue, (void*)&ucInput, &xHigherPriorityTaskWoken);
    }
    else
    {
        xQueueSendFromISR(xArbWaveInputQueue, (void*)&ucInput, &xHigherPriorityTaskWoken);
    }
}