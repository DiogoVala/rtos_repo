/*
 * 18-Jun-2021
 * Beatriz Silva 85276
 * Diogo Vala 64671
 *
 *******************************************************************************
 * Arbitrary waveform generator
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

/* PWM signal to be applied to filter*/
#define mainAwgPWM_FREQUENCY 158102 /* Frequency that sets PR2 to 254 (duty steps) - 1 , for exact duty cycle steps */
#define mainAwgSELECTED_TIMER 2 /* Timer 2 as source for OC1 */

#define mainAwgWAVEFORM_SIZE 400 /* Number of duty cycle samples per period of output signal*/

/* Max vars of system */
#define mainAwgMAX_FREQUENCY 300 /* 0 - 300 Hz*/
#define mainAwgMAX_AMPLITUDE 33 /* 0 - 3.3 V */
#define mainAwgMAX_PHASE 360 /* 0 - 360 Deg */
#define mainAwgMAX_DUTY 100 /* 0 - 100 % */

#define mainAwgINPUT_BUFFER_SIZE 6 /* Max number of bytes to add to input buffer */

/* User defined variables */
static volatile uint8_t ucWavetype; /* 0: sine; 1: square; 2: sawtooth*/
static volatile uint16_t usFrequency = 0; /* Desired output signal frequency*/
static volatile uint8_t ucWaveAmplitude = 0; /* Desired output signal Amplitude 0-33 (0-3.3V) */
static volatile uint16_t usPhase = 0; /* Desired output signal phase*/
static volatile uint8_t usDuty = 50; /* Desired output signal duty cycle ( for square and triangle) */

/* Arrays to store duty cycle samples */
static volatile uint8_t usWaveform[mainAwgWAVEFORM_SIZE] = {0}; /* Array to store waveform duty cycle samples */
static volatile uint8_t usArbitraryWaveform[mainAwgWAVEFORM_SIZE] = {0}; /* Array to store arbitrary waveform */

/* Internal logic */
static volatile uint16_t usPhaseIndex = 0; /* Starting index for the duty cycle samples, according to desired phase*/
static volatile uint16_t usWaveIndex = 0; /* Index of usWaveform[] to send to OC1 */
static volatile uint16_t usArbWaveIndex = 0; /* Index of arbitrary wave array */
static volatile uint8_t usMaxDutycycle = 0; /* Maximum value of duty cycle according to desired amplitude */

static volatile bool ucIsCommand = true; /* To distinguish between normal command or waveform file input*/

/* Queue Handles */
QueueHandle_t xInputQueue = NULL;
QueueHandle_t xArbWaveInputQueue = NULL;

/* Wave types */
enum WaveForm_t{
    WAVE_OFF,
    WAVE_SINE,
    WAVE_TRIANGLE,
    WAVE_SQUARE,
    WAVE_ARBITRARY
};

/* Task handles */
static TaskHandle_t xSequencer = NULL, xInterface = NULL, xWaveformGenerator= NULL, xLoadWave = NULL;

/*
 * Prototypes and tasks
 */

/* Task called when system is receiving an arbitrary waveform file 
 * 
 * Takes each byte from UART and stores them in usArbitraryWaveform[].
 * Executes upon the existence of a byte in Queue
 */
void pvLoadWaveform( void *pvParam)
{
    uint8_t ucRxInput = '\0';
            
    while(1)
    {
        xQueueReceive( xArbWaveInputQueue, (uint8_t*)&ucRxInput, portMAX_DELAY);
        
        Timer3Stop();

        usArbitraryWaveform[usArbWaveIndex]=ucRxInput;
        usArbWaveIndex++;
    }
}

/* Task called when a new byte is received from UART
 * 
 * Evaluates each byte and constructs a command sequence.
 * Updates system variables according to the command sequence.
 * Executes upon the existence of a byte in Queue
 */
void pvInterface(void *pvParam)
{    
    static uint8_t ucBuffer[mainAwgINPUT_BUFFER_SIZE]={'0'};
    static uint8_t ucBufIdx=0;
        
    uint8_t  ucRxInput = '\0';
    uint8_t  ucCommand = '\0'; /* First byte of the buffer indicates the command */
    
    uint32_t usNewFrequency;
    uint32_t ucNewWaveAmplitude;
    uint32_t usNewPhase;
    uint32_t usNewDuty;
    
    while(1) {
        
        xQueueReceive( xInputQueue, (uint8_t*)&ucRxInput, portMAX_DELAY);
        
        /* Reset input buffer and current queue*/
        if(ucRxInput == 'r')
        {
            printf("\r\n");
            ucBufIdx=0;
            xQueueReset(xArbWaveInputQueue);
            memset(ucBuffer, '\0', mainAwgINPUT_BUFFER_SIZE);
        }
        /* Put valid bytes into input buffer */
        else if((ucRxInput >= '0'  && ucRxInput <= '9' ) || (ucRxInput >= 'A'  && ucRxInput <= 'Z' )\
                || (ucRxInput >= 'a'  && ucRxInput <= 'z' )){
            
            printf("%c", ucRxInput);
            ucBuffer[ucBufIdx]=ucRxInput;
            ucBufIdx++;
            if(ucBufIdx>=mainAwgINPUT_BUFFER_SIZE-1){
                ucBufIdx=0;
                memset(ucBuffer, '\0', mainAwgINPUT_BUFFER_SIZE);
                printf("\rInvalid command.\n");
            }  
        }
        /* On New line, evaluate buffer and update variables */
        else if(ucRxInput=='\r')
        {
            printf("\r\n\n");
            
            ucCommand=ucBuffer[0];
            
            Timer3Stop(); /* Stop wave output */
            OC1SetDutyCycle(0);
            
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
                case 'd':
                case 'D':
                    usNewDuty = (ucBuffer[1]-'0')*100+(ucBuffer[2]-'0')*10+(ucBuffer[3]-'0');
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
           
            if(usNewFrequency == 0)
            {
                ucWavetype=WAVE_OFF;
            }
            else if(usNewFrequency<=mainAwgMAX_FREQUENCY)
            {
                usFrequency=(uint16_t)usNewFrequency;
                printf("\rFreq: %d Hz\n", usFrequency);
            }
            else
            {
                printf("\rInvalid frequency.\n");
            }

            if(ucNewWaveAmplitude == 0)
            {
                ucWavetype=WAVE_OFF;
            }
            else if(ucNewWaveAmplitude<=mainAwgMAX_AMPLITUDE)
            {
                ucWaveAmplitude = (uint8_t)ucNewWaveAmplitude;
                usMaxDutycycle = ucWaveAmplitude*oc1MAX_DUTYCYCLE/mainAwgMAX_AMPLITUDE;
                printf("\rAmp: %d.%d V\n", ucWaveAmplitude/10,ucWaveAmplitude%10);
            }
            else
            {
                printf("\rInvalid amplitude.\n");
            }

            if(usNewPhase<=mainAwgMAX_PHASE)
            {
                if(usNewPhase==mainAwgMAX_PHASE)
                {
                    usNewPhase=0;
                }
                usPhase = (uint16_t)usNewPhase;
                usPhaseIndex = usPhase*mainAwgWAVEFORM_SIZE/mainAwgMAX_PHASE;
                printf("\rPhase: %d Deg\n", usPhase);
            }
            else
            {
                printf("\rInvalid phase.\n");
            }
            
            if(usNewDuty<=mainAwgMAX_DUTY)
            {
                usDuty = (uint8_t)usNewDuty;
                printf("\rDuty: %d %%\n\n", usDuty); 
            }
            else
            {
                printf("\rInvalid duty.\n");
            }
            
            xTaskNotifyGive(xWaveformGenerator);
        }
    }
}

/* Task called to generate usWaveform[] array of samples for desired output signal 
 * 
 * Generates one period of the signal with the desired amplitude
 * Executes on notification from the Interface Task
 */
void pvWaveformGenerator(void *pvParam)
{
    uint16_t usIterator;
    
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            
        uint16_t usDutyIndex=(usDuty*mainAwgWAVEFORM_SIZE/mainAwgMAX_DUTY);
        
        switch(ucWavetype){
                
            case WAVE_SINE:
                for(usIterator = 0; usIterator < mainAwgWAVEFORM_SIZE; usIterator++)
                {
                    usWaveform[usIterator]=usMaxDutycycle/2;
                    if(usIterator < usDutyIndex/2)
                    {
                        usWaveform[usIterator]=usWaveform[usIterator]=(uint8_t)ceil(((float)usMaxDutycycle/2*sin((2*M_PI*(usIterator+1))/mainAwgWAVEFORM_SIZE)+usMaxDutycycle/2));
                    }
                    if(usIterator > mainAwgWAVEFORM_SIZE/2 && usIterator < (mainAwgWAVEFORM_SIZE/2 + usDutyIndex/2))
                    {
                        usWaveform[usIterator]=usWaveform[usIterator]=(uint8_t)ceil(((float)usMaxDutycycle/2*sin((2*M_PI*(usIterator+1))/mainAwgWAVEFORM_SIZE)+usMaxDutycycle/2));
                    }  
                }
                break;
                
            case WAVE_SQUARE:
                for(usIterator = 0; usIterator < mainAwgWAVEFORM_SIZE; usIterator++)
                {
                    if(usIterator < usDutyIndex)
                    {
                        usWaveform[usIterator]=usMaxDutycycle;
                    }
                    else
                    {
                        usWaveform[usIterator]=0;
                    }
                }
                break;
                
            case WAVE_TRIANGLE:
                for(usIterator = 0; usIterator < mainAwgWAVEFORM_SIZE; usIterator++)
                {
                    if(usIterator < (usDuty*mainAwgWAVEFORM_SIZE/mainAwgMAX_DUTY))
                    {
                        usWaveform[usIterator]=(uint8_t)(usIterator*usMaxDutycycle/usDutyIndex);
                    }
                    else
                    {
                        usWaveform[usIterator]=usMaxDutycycle-usMaxDutycycle*(usIterator-usDutyIndex)/(mainAwgWAVEFORM_SIZE-usDutyIndex);
                    }
                    
                }
                break;
                
            case WAVE_ARBITRARY:
                for(usIterator = 0; usIterator < mainAwgWAVEFORM_SIZE; usIterator++)
                {
                    usWaveform[usIterator]=(uint8_t)(usArbitraryWaveform[usIterator]*ucWaveAmplitude/mainAwgMAX_AMPLITUDE);
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
    void __attribute__( (interrupt(IPL4AUTO), vector(_UART_1_VECTOR))) vU1InterruptWrapper(void);
    
    /* PWM Timer and OC */
    Timer2Config(mainAwgPWM_FREQUENCY);
    OC1Config(mainAwgSELECTED_TIMER);
    OC1Control(oc1START);
    Timer2Start();
    OC1SetDutyCycle(0);
     
    /* Set RD0 as digital output (OC1) */
    TRISDbits.TRISD0 = 0;
    PORTDbits.RD0 = 1;
    
    /* Set RE8 as digital output (External Marker) */
    TRISEbits.TRISE8 = 0;
    PORTEbits.RE8 = 0;
    
	/* Init UART and redirect tdin/stdot/stderr to UART */
    if(UartInit(configPERIPHERAL_CLOCK_HZ, 115200) != UART_SUCCESS) {
        PORTAbits.RA3 = 1; // If Led active error initializing UART
        while(1);
    }
    U1STAbits.URXISEL = 0; /* Interrupt on each new byte */
    IPC6bits.U1IP = 4; /* Interrupt priority */
    IEC0bits.U1RXIE = 1; /* Enable receive interupts */
    IFS0bits.U1RXIF = 0; /* Clear the rx interrupt flag */
    __XC_UART = 1; /* Redirect stdin/stdout/stderr to UART1 */
    
    /* Initial Menu */
    printf("\033[2J");
    printf("\rArbitrary Waveform Generator - Diogo Vala & Beatriz Silva\n");
    printf("\rCommands: \n");
    printf("\rs (sine); t(triangle); q(square); a(arbitrary)\n");
    printf("\rFrequency: Fxxx -> 000-300\n");
    printf("\rAmplitude: Vxx -> 00-33\n");
    printf("\rPhase: Pxxx -> 000-360\n");
    printf("\rDuty: Dxxx -> 000-100\n");
    
    /* Queue Creation */
    xInputQueue = xQueueCreate(mainAwgINPUT_BUFFER_SIZE, sizeof(uint8_t));
    xArbWaveInputQueue = xQueueCreate(mainAwgWAVEFORM_SIZE, sizeof(uint8_t));
    
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

/* Timer3 ISR 
 * 
 * Updates the duty cycle value to be applied to OC1
 * External marker is set to 1 at the start of the period and
 * set back to 0 after mainAwgMARKER_WIDTH samples.
 * 
 * Frequency of timer 3 is mainAwgWAVEFORM_SIZE*ucFrequency,
 * updates upon every new command.
 * 
*/
void vT3InterruptHandler(void)
{
    static const uint16_t mainAwgMARKER_WIDTH = mainAwgWAVEFORM_SIZE/20; /* Number of samples that the trigger stays HIGH*/
    
    /* Update duty cycle */
    OC1SetDutyCycle(usWaveform[usWaveIndex]);

    /* External Marker */
    if(usWaveIndex>=usPhaseIndex && usWaveIndex<=(usPhaseIndex+mainAwgMARKER_WIDTH))
    {
        LATEbits.LATE8 = 1;
    }
    else
    {
        LATEbits.LATE8 = 0;
    }
    
    /* Repeat period */
    usWaveIndex++;
    if(usWaveIndex>=mainAwgWAVEFORM_SIZE)
    {
        usWaveIndex=0;
    }
    
    Timer3ResetInterruptFlag();
}

/* UART ISR 
 * 
 * Receives serial inputs byte by byte.
 *  
 * Checks if the input is 'l', that signifies the start of an arbitrary wave
 * input.
 * Checks if the input is 0xFF, that signifies the end of transfer for the
 * arbitrary wave
 * 
 * Sends the received byte to either the Interface or LoadWave task, according
 * to a flag, via two queues.
 */
void vU1InterruptHandler(void) {
   
    /* Received byte */
    uint8_t ucTxInput=0;
    uint8_t ucTrash;
    bool ucHasRxed = false;
    
    if(U1STAbits.OERR ||U1STAbits.FERR || U1STAbits.PERR) // receive errors?
	{
		ucTrash = U1RXREG; /* dummy read to clear FERR/PERR */
		U1STAbits.OERR = 0; /* clear OERR to keep receiving */
	}
	if(U1STAbits.URXDA)
	{
		ucTxInput = U1ARXREG; /* get data from UART RX FIFO */
        ucHasRxed=true;
	}
    
    if(ucTxInput=='l')
    {
        ucIsCommand=false; /* Next inputs will not be commands, send them to LoadWave task */
        usArbWaveIndex = 0; /* Set usArbitraryWaveform[] to index 0 to load new wave*/
    }
    else if(ucTxInput==0xFF) /* End of wave input */
    {
        Timer3Start();
        ucIsCommand=true; /* Next inputs are commands, send them to interface task */
    }
    
    IFS0bits.U1RXIF = 0; /* clear the RX interrupt flag */
    
    if(ucHasRxed){
        if(ucIsCommand)
        {
            if(xQueueSendFromISR(xInputQueue, (void*)&ucTxInput, NULL) != pdTRUE )
            {
                printf("\rERROR: INPUT QUEUE FULL.\n");
            }
        }
        else
        {
            if(xQueueSendFromISR(xArbWaveInputQueue, (void*)&ucTxInput, NULL) != pdTRUE )
            {
                printf("\rERROR: ARBITRARY WAVE QUEUE FULL.\n");
            }
        }
    }
}