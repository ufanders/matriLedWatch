/***********************************************************************
CHANGELOG

AUTHOR			DATE		COMMENTS
ANDERS NELSON	2010.04.29	FILE CREATED
ANDERS NELSON	2010.04.30	ADDED REALTIME CLOCK PORTION
ANDERS NELSON	2010.05.03a	TESTED LED MATRIX, FINISHED SCAN MECHANISM,
							CORRECTED UNINITIALIZED ROW VALUE
ANDERS NELSON	2010.05.03b	ADDED PER-ENTIRE-COLOR-FRAME PWM
ANDERS NELSON	2010.05.03c ADDED SIMULTANEOUS 3-COLORS ROW PWM
ANDERS NELSON	2010.05.04a CHANGED BITMAP STRUCTURE FOR SPEED,
							ADDED ACCELEROMETER FUNCTIONS, ADDED RTC
ANDERS NELSON	2010.05.05a 
ANDERS NELSON	2010.05.07a ADDED FONT, BEGAN MORE GFX FCNS
ANDERS NELSON	2010.05.08a ADDED BITMAP DRAW CHAR, FONT PTR
ANDERS NELSON	2010.05.09a TESTED DRAWPIXEL, FIXED DRAWCHAR
ANDERS NELSON	2010.05.11a ADDED ANIMATED TIME DISPLAY, GETTIME(),
							SHOWTIME(), STATE MACHINE, TICK TIMER
ANDERS NELSON	2010.05.17a MOVED GETTIME() INTO TMR1 ISR, FIXED
							TMR1 PEIE PROBLEM
ANDERS NELSON	2010.05.18a ADDED BATTERY STATUS INIT AND ISR, ADDED
							BITMAPGETPIXEL(), FIXED BUTTON ISR BOUNCE
ANDERS NELSON	2010.05.20a ADDED SETTIME(), NEED TO REVERT TO TWO-
							DIGIT FIELD ENTRY
ANDERS NELSON	2010.05.21a FINISHED TIME ENTRY, ADDED SHUTDOWN
							ANIMATION
ANDERS NELSON	2010.05.28a ADDED PROVISONS FOR BATT CHECK,
							ADDED CLOCK SPEED VARIATION
***********************************************************************/
#include <pic.h>

__CONFIG(FOSC_INTOSC & WDTE_OFF & PWRTE_OFF & MCLRE_ON & CP_OFF & CPD_OFF & BOREN_OFF & CLKOUTEN_OFF); // Program config. word 1
__CONFIG(WRT_OFF & VCAPEN_OFF /*why does the pin diagram show this as RF0?*/ & PLLEN_OFF & STVREN_OFF & LVP_OFF /*& DEBUG_ON*/); // Program config. word 2

// This definition is required to calibrate __delay_us() and __delay_ms()
#define _XTAL_FREQ 32000000

//main stuff
#define SW1_TRIS TRISBbits.TRISB2
#define SW2_TRIS TRISBbits.TRISB3
#define SW3_TRIS TRISBbits.TRISB4
#define SW1 PORTBbits.RB2
#define SW2 PORTBbits.RB3
#define SW3 PORTBbits.RB4

#define SW_DEBOUNCE 40000

#define VLED_EN_TRIS TRISBbits.TRISB5
#define VLED_EN LATBbits.LATB5
#define VLED_TRIS TRISGbits.TRISG2
#define VLED LATGbits.LATG2

#define BATT_STAT1_TRIS TRISBbits.TRISB1
#define BATT_STAT1 PORTBbits.RB1
#define BATT_VSENSE_GND_TRIS TRISGbits.TRISG3
#define BATT_VSENSE_GND LATGbits.LATG3
#define BATT_VSENSE_TRIS TRISGbits.TRISG4
#define BATT_VSENSE LATGbits.LATG4

//TODO: enumerate these states
#define state_idle 0
#define state_getTime 1
#define state_setTime 2
#define state_showTime 3
#define state_sleep 4
#define state_battWarning 5

#define state_setTime_idle 0
#define state_setTime_year 1
#define state_setTime_month 2
#define state_setTime_day 3
#define state_setTime_hour 4
#define state_setTime_minute 5
#define state_setTime_second 6

#define state_showTime_idle 0
#define state_showTime_splash 1
#define state_showTime_time 2
#define state_showTime_date 3

volatile unsigned char currentState;
volatile unsigned int timerTicks;
unsigned char updateDisplayRequest;

unsigned char init(void);
unsigned char getBattery(void);
/*
unsigned char sw1IsPressed(void);
unsigned char sw2IsPressed(void);
unsigned char sw3IsPressed(void)
*/

//display stuff
#include <stdlib.h>
#include "2010_05_07_font5x7.h"
const unsigned char * fontPtr = &font[0];

#define MATRIX_COLUMNS 8
#define MATRIX_ROWS 8
#define MATRIX_COLORS 3
#define MATRIX_REFRESH_FPS 60
#define MATRIX_PWM_MAX 15 //4096 colors
#define MATRIX_SCAN_TIMER_FCY 8000000
#define MATRIX_SCAN_TIMER_PRESCALE 8
#define MATRIX_SCAN_TIMER_MAX 0xFF
#define MATRIX_OPS_PER_SECOND (MATRIX_ROWS*MATRIX_REFRESH_FPS*MATRIX_PWM_MAX)
#define MATRIX_SCAN_TIMER_PRELOAD (MATRIX_SCAN_TIMER_MAX - ((MATRIX_SCAN_TIMER_FCY/MATRIX_SCAN_TIMER_PRESCALE)/MATRIX_OPS_PER_SECOND))

#define MATRIX_TRIS_ROW_1 TRISGbits.TRISG0
#define MATRIX_TRIS_ROW_2 TRISFbits.TRISF1
#define MATRIX_TRIS_ROW_3 TRISFbits.TRISF2
#define MATRIX_TRIS_ROW_4 TRISFbits.TRISF3
#define MATRIX_TRIS_ROW_5 TRISFbits.TRISF4
#define MATRIX_TRIS_ROW_6 TRISFbits.TRISF5
#define MATRIX_TRIS_ROW_7 TRISFbits.TRISF6
#define MATRIX_TRIS_ROW_8 TRISFbits.TRISF7

#define MATRIX_LAT_ROW_1 LATGbits.LATG0
#define MATRIX_LAT_ROW_2 LATFbits.LATF1
#define MATRIX_LAT_ROW_3 LATFbits.LATF2
#define MATRIX_LAT_ROW_4 LATFbits.LATF3
#define MATRIX_LAT_ROW_5 LATFbits.LATF4
#define MATRIX_LAT_ROW_6 LATFbits.LATF5
#define MATRIX_LAT_ROW_7 LATFbits.LATF6
#define MATRIX_LAT_ROW_8 LATFbits.LATF7

#define MATRIX_TRIS_COLOR_RED TRISA
#define MATRIX_LAT_COLOR_RED LATA
#define MATRIX_TRIS_COLOR_GRN TRISD
#define MATRIX_LAT_COLOR_GRN LATD
#define MATRIX_TRIS_COLOR_BLU TRISE
#define MATRIX_LAT_COLOR_BLU LATE

/*
typedef struct
{
	unsigned r:8;
	unsigned g:8;
	unsigned b:8;
} color_t;
*/

//pwm byte array is now linear, (R,G,B) byte interleaved, most significant pixel first
//i.e. R7,G7,B7,R6,G6,B6,R5,G5,B5,...
unsigned char bitmap[MATRIX_COLUMNS*MATRIX_ROWS*MATRIX_COLORS]; //each pixel color allocated a byte for PWM value
volatile near unsigned char currentRefreshingColor, currentRefreshingRow, currentRefreshingRowIndex, currentRefreshingPwmSweep;
//color_t color;

void matrixTest(void);
void bitmapTest(void);
void bitmapDrawPixel(unsigned char, unsigned char, unsigned char, unsigned char, unsigned char);
void bitmapGetPixel(unsigned char x, unsigned char y, unsigned char *, unsigned char *, unsigned char *);
void bitmapDrawChar(signed char, signed char, unsigned char, unsigned char, unsigned char, unsigned char);
void bitmapClear(void);
void showTime(void);
void setTime(void);

//RTC stuff
#include "Rtc.h"
volatile near unsigned char tickCounter, newSecond;
volatile near unsigned int tmr1Value;
volatile near unsigned char hours, minutes, seconds;
volatile near bit ampm;

//volatile near timeDate_t timeDate;

void getTime(void);

//accelerometer stuff
#include "BMA150.h"

void main(void)
{	
	//init system
	if(init())
	{
		while(1); //couldn't initialize - halt
	}
	
	GIE = 1; // Global interrupt enable
		
	while(1)
	{
		//TODO: check battery level and change state if necessary

		//if battery >3.5v, speed up. If not, give indication.
		//(3.5v/4.2v = 0.83). (0.83*248 = 206).
		//Lower 2 ADC bits are of inconsequential value.
		if(getBattery() < 206)
		{
			currentState = state_battWarning;
		}
		
		switch(currentState)
		{
			case state_idle:
			break;
			
			case state_getTime: //grab the time!
			//getTime(); //this has been moved into ISR
			currentState = state_sleep;
			break;
			
			case state_setTime: //set the time!
			IOCIE = 0; // disable CN
			//TODO: engage PLL, switch to HFINTOSC
			setTime();
			//setOptions();
			currentState = state_showTime;
			break;
			
			case state_showTime: //display the time!
			IOCIE = 0; // disable CN
			//TODO: engage PLL, switch to HFINTOSC
			showTime(); //display the time!
			currentState = state_sleep;
			break;
			
			case state_sleep: //time to sleep!
			
			//TODO: kill refresh timer
			TMR0IE = 0;

			//TODO: ground display row sources
			MATRIX_LAT_ROW_1 = MATRIX_LAT_ROW_2 = \
			MATRIX_LAT_ROW_3 = MATRIX_LAT_ROW_4 = \
			MATRIX_LAT_ROW_5 = MATRIX_LAT_ROW_6 = \
			MATRIX_LAT_ROW_7 = MATRIX_LAT_ROW_8 = 0;
			
			//TODO: ground display column sinks
			MATRIX_LAT_COLOR_RED = MATRIX_LAT_COLOR_GRN = MATRIX_LAT_COLOR_BLU = 0;
			
			//TODO: kill display supply
			VLED_EN = 0;
			
			//TODO: put serial flash to sleep
			
			//TODO: put accelerometer to sleep?
			
			//TODO: kill PLL, switch to MFINTOSC
			OSCCON = 0b01101001; //PLL off, 1MHz HF, system clocked by HFINTOSC oscillator
			
			IOCBF = 0; // clear button interrupts
			IOCIF = 0; // clear button interrupts
			IOCIE = 1; //re-enable button interrupts
			
			asm("SLEEP");
			asm("NOP");
			//interrupt (buttons, timekeeping) will set appropriate state
			
			break;
			
			case state_battWarning: //indicate the battery is low
			
			
			break;
			
			default:
			break;
		}
	}	
}

unsigned char init(void)
{
	//init CPU clock
	//OSCCON = 0b01111010; //PLL off, 16MHz HF, system clocked by HFINTOSC block
	OSCCON = 0b11110000; //PLL on, 8MHz HF, system clocked by HFINTOSC block
	while(!OSCSTATbits.PLLR); //wait for PLL to be ready
	
	//init RTC
	T1CON = 0b10001100; //T1OSC selected, 1:1 prescale, T1OSC on, timer1 off
	while(!OSCSTATbits.T1OSCR); //wait for timer1 oscillator to be ready
	TMR1 = 0;
	TMR1IF = 0; //clear interrupt flag
	TMR1IE = 1;	//enable interrupt on TMR0 overflow
	__delay_ms(1); //wait for timer1 oscillator to stabilize
	T1CONbits.TMR1ON = 1; //timer1 on
	PEIE = 1; //enable peripheral interrupts
	
	hours = START_H;
	minutes = START_M;
	seconds = START_S;
	ampm = START_AP;
	
	newSecond = tickCounter = 0;
	
	//init display
	////gpio
	ANSELA = ANSELE = ANSELF = ANSELG = 0; //analog gpio as digital inputs
	
	MATRIX_LAT_COLOR_RED = ~0x00; //gpio high (columns off) by default (these are RED column current sinks)
	MATRIX_LAT_COLOR_GRN = ~0x00; //gpio high (columns off) by default (these are GREEN column current sinks)
	MATRIX_LAT_COLOR_BLU = ~0x00; //gpio high (columns off) by default (these are BLUE column current sinks)
	MATRIX_TRIS_COLOR_RED = 0x00; //gpio as output
	MATRIX_TRIS_COLOR_GRN = 0x00; //gpio as output
	MATRIX_TRIS_COLOR_BLU = 0x00; //gpio as output
	
	MATRIX_LAT_ROW_1 = 0; //gpio low (rows off) by default (this is row current driver 1)
	MATRIX_LAT_ROW_2 = 0; //gpio low (rows off) by default (this is row current driver 2)
	MATRIX_LAT_ROW_3 = 0; //gpio low (rows off) by default (this is row current driver 3)
	MATRIX_LAT_ROW_4 = 0; //gpio low (rows off) by default (this is row current driver 4)
	MATRIX_LAT_ROW_5 = 0; //gpio low (rows off) by default (this is row current driver 5)
	MATRIX_LAT_ROW_6 = 0; //gpio low (rows off) by default (this is row current driver 6)
	MATRIX_LAT_ROW_7 = 0; //gpio low (rows off) by default (this is row current driver 7)
	MATRIX_LAT_ROW_8 = 0; //gpio low (rows off) by default (this is row current driver 8)
	MATRIX_TRIS_ROW_1 = 0; //gpio as output
	MATRIX_TRIS_ROW_2 = 0; //gpio as output
	MATRIX_TRIS_ROW_3 = 0; //gpio as output
	MATRIX_TRIS_ROW_4 = 0; //gpio as output
	MATRIX_TRIS_ROW_5 = 0; //gpio as output
	MATRIX_TRIS_ROW_6 = 0; //gpio as output
	MATRIX_TRIS_ROW_7 = 0; //gpio as output
	MATRIX_TRIS_ROW_8 = 0; //gpio as output

	////matrix scan variables
	currentRefreshingColor = currentRefreshingRow = \
	currentRefreshingRowIndex = currentRefreshingPwmSweep = 0;
	
	bitmapClear();
	updateDisplayRequest = 0;
	
	////matrix scan timer
	//TODO: consider using timer2/4/6 with auto-clear and period register
	OPTION_REG = 0b00001010; //WPU individual, falling edge, fosc/4, 1:8 prescale
	TMR0 = MATRIX_SCAN_TIMER_PRELOAD;
	TMR0IF = 0; //clear interrupt flag
	TMR0IE = 1;	//enable interrupt on TMR0 overflow
	
	//init accelerometer
	InitBma150();
	
	//init serial flash
	
	//init battery charging
	BATT_STAT1_TRIS = 1; //battery status line as input
	WPUB |= 0b00000010; //battery status line pulled up (BATT_STAT1 is open collector on charger)
	IOCBP |= 0b00000010; //RB1 enabled as positive-edge interrupt-on-change pin
	IOCBN |= 0b00000010; //RB1 enabled as negative-edge interrupt-on-change pin
	
	//init battery level sensing
	BATT_VSENSE_TRIS = 1; //battery voltage sense as input
	BATT_VSENSE_GND_TRIS = 1; //battery voltage divider bottom as Hi-Z
	BATT_VSENSE_GND = 0; //preload battery voltage divider bottom to sink current
	
	FVRCON = 0b00000010; //disable FVR, comparator/DAC not fed, ADC fed with 2.048v.
	ADCON1 = 0b00000011; //left justified, FOSC/2, Vref- = Vss, Vref+ = FVR.
	ADCON0 = 0b00110000; //AN12, ADC OFF
	
	//init user interface
	//WPUB = 0b00011100; //enable internal pullups for all IOC pins (pulled up by resistors on Rev A board)
	SW1_TRIS = 1;
	SW2_TRIS = 1;
	SW3_TRIS = 1;
	IOCBN = 0b00011100; //RB4-RB2 enabled as negative-edge interrupt-on-change pins
	IOCBF = 0; //clear all pin-change interrupt flags
	IOCIF = 0; //clear the interrupt flag
	IOCIE = 1; //enable interrupt-on-change
	
	////generic 1ms tick timer
	T2CON = 0b00000011; //1:1, OFF, 1:64
	PR2 = 125; //makes for interval of 1ms
	TMR2IF = 0; //clear interrupt flag
	TMR2IE = 1; //enable timer2 interrupt
	
	//init state machine
	currentState = state_setTime;
	
	return 0;
}	

static void interrupt isr(void) // Here be interrupt function - the name is unimportant.
{
	if((TMR0IE)&&(TMR0IF)) // matrix scan timer interrupt
	{
		unsigned char rowValueTempR, rowValueTempG, rowValueTempB, currentRefreshingColumn;
		rowValueTempR = rowValueTempG = rowValueTempB = 0;
		
		//using pointers and addition instead of 3-dimensional reference to avoid multiplication
		
		//for the current row
		for(currentRefreshingColumn = 0; currentRefreshingColumn < MATRIX_COLUMNS; currentRefreshingColumn++)
		{
			if(bitmap[currentRefreshingRowIndex++] > currentRefreshingPwmSweep)
			{
				rowValueTempR |= 0x80;
			}
			if(bitmap[currentRefreshingRowIndex++] > currentRefreshingPwmSweep)
			{
				rowValueTempG |= 0x80;
			}
			if(bitmap[currentRefreshingRowIndex++] > currentRefreshingPwmSweep)
			{
				rowValueTempB |= 0x80;
			}
			
			if(currentRefreshingColumn < (MATRIX_COLUMNS-1))
			{
				rowValueTempR >>= 1;
				rowValueTempG >>= 1;
				rowValueTempB >>= 1;
			}	
		}
		
		//we're blanking the display at the last possible moment to maximize on-time
		//and thus display brightness at these high scan rates.
		
		//blank display
		MATRIX_LAT_ROW_1 = MATRIX_LAT_ROW_2 = \
		MATRIX_LAT_ROW_3 = MATRIX_LAT_ROW_4 = \
		MATRIX_LAT_ROW_5 = MATRIX_LAT_ROW_6 = \
		MATRIX_LAT_ROW_7 = MATRIX_LAT_ROW_8 = 0;
		
		//for all colors simultaneously
		MATRIX_LAT_COLOR_RED = ~rowValueTempR; //set column sinks
		MATRIX_LAT_COLOR_GRN = ~rowValueTempG; //set column sinks
		MATRIX_LAT_COLOR_BLU = ~rowValueTempB; //set column sinks

		//for the current row
		switch(currentRefreshingRow)
		{
			case 0:
			MATRIX_LAT_ROW_1 = 1;
			break;
			
			case 1:
			MATRIX_LAT_ROW_2 = 1;
			break;
			
			case 2:
			MATRIX_LAT_ROW_3 = 1;
			break;
			
			case 3:
			MATRIX_LAT_ROW_4 = 1;
			break;
			
			case 4:
			MATRIX_LAT_ROW_5 = 1;
			break;
			
			case 5:
			MATRIX_LAT_ROW_6 = 1;
			break;
			
			case 6:
			MATRIX_LAT_ROW_7 = 1;
			break;
			
			case 7:
			MATRIX_LAT_ROW_8 = 1;
			break;
			
			default:
			break;
		}	
		
		currentRefreshingRow++;
		if(currentRefreshingRow > MATRIX_ROWS) //when all rows have been scanned
		{
			currentRefreshingRow = 0;
		}
		
		//currentRefreshingRowIndex++;
		if(currentRefreshingRowIndex > (MATRIX_COLUMNS*MATRIX_ROWS*MATRIX_COLORS)) //when all color rows have been scanned
		{
			currentRefreshingRowIndex = 0; //reset to first color row
			
			//for the current PWM sweep
			currentRefreshingPwmSweep++;
			if(currentRefreshingPwmSweep > MATRIX_PWM_MAX) //when all pwm cycles have passed
			{
				currentRefreshingPwmSweep = 0; //reset to first pwm sweep
			}
		}	

		TMR0 = MATRIX_SCAN_TIMER_PRELOAD; //reset the timer to fire at the proper interval
		TMR0IF = 0;			// clear the interrupt flag
	}
	
	if((TMR1IE)&&(TMR1IF))//RTC timer interrupt
	{
		//At 8Mhz Tcy, we have 244 instruction periods before the RTC oscillator ticks once.
		//This means we don't need to worry too much about how many cycles it takes to check
		//the TMR1 register pair and set it to overflow 32,768 ticks after the last ISR.
		
		tmr1Value = TMR1; //take note of current value
		TMR1 = (0x8000 - tmr1Value); //subtract current value from 32,768 ticks to maintain accuracy

		if(++seconds > 59)
		{
			seconds = 0;
			
			if(++minutes > 59)
			{
				minutes = 0;
				hours++;
				if(hours == 12) ampm ^= 1;
				if(hours>12) hours = 1;
			}
		}
		
		currentState = state_getTime; //update the central timekeeping values
	
		TMR1IF = 0;			// clear the interrupt flag
	}
	
	if((TMR2IE)&&(TMR2IF))//generic timer interrupt
	{
		timerTicks++;
		//TMR2 = 0; //reset the timer to fire at the proper interval (should be auto-cleared)
		
		TMR2IF = 0;	// clear the interrupt flag
	}
	
	if((IOCIE)&&(IOCIF))//buttons interrupt
	{
		if(IOCBFbits.IOCBF4) //SW3 ISR (SHOW TIME BUTTON)
		{
			switch(currentState)
			{
				case state_setTime:
					//currentState = state_showTime;
				break;
				
				case state_showTime:
					//currentState = state_showTime;
				break;
				
				case state_idle:
					currentState = state_showTime;
				break;
				
				case state_sleep:
					currentState = state_showTime;
				break;
				
				default:
				break;
			}	
		}
		
		
		if((IOCBFbits.IOCBF3) /*&& !timeEntryWaiting*/) //SW2 ISR (ADJUST + BUTTON)
		{
			/*
			if(currentState == state_idle)
			{
				timeEntryWaiting = 1;
				
				//start timer counting up to 2 seconds
				TMR1 = 0; //reload timer
				T1CON = 0b1000000000000010; //SOSC, 1:1	, TIMER1 ON
				IEC0bits.T1IE = 1; //TIMER1 interrupt enabled
			}*/
			
			if(currentState == state_sleep)
			{
				currentState = state_setTime;
			}	
		}
		
		/*
		//only the buttons above are used to wakeup the system from sleep
		if(IOCBFbits.IOCBF2) //SW3 ISR (ADJUST - BUTTON)
		{
			
		}
		*/
		
		if(IOCBFbits.IOCBF1) //BATTERY STATUS ISR
		{
			//TODO: monitor time interval between triggers to see if there is a fault
			
			if(BATT_STAT1) //TODO: high means charging
			{
				
			}
			else //TODO: low means not charging
			{
				
			}	
			
		}
		
		IOCBF = 0; //clear individual pin-change flags (this clears all of them)
		IOCIF = 0; //clear the interrupt flag
	}
}

void matrixTest(void)
{
	MATRIX_LAT_COLOR_RED = ~0xFF; //sink current into RED columns
	
	MATRIX_LAT_ROW_1 = 1; //send current to row
	MATRIX_LAT_ROW_1 = 0; //recind current to row
	MATRIX_LAT_ROW_2 = 1; //send current to row
	MATRIX_LAT_ROW_2 = 0; //recind current to row
	MATRIX_LAT_ROW_3 = 1; //send current to row
	MATRIX_LAT_ROW_3 = 0; //recind current to row
	MATRIX_LAT_ROW_4 = 1; //send current to row
	MATRIX_LAT_ROW_4 = 0; //recind current to row
	MATRIX_LAT_ROW_5 = 1; //send current to row
	MATRIX_LAT_ROW_5 = 0; //recind current to row
	MATRIX_LAT_ROW_6 = 1; //send current to row
	MATRIX_LAT_ROW_6 = 0; //recind current to row
	MATRIX_LAT_ROW_7 = 1; //send current to row
	MATRIX_LAT_ROW_7 = 0; //recind current to row
	MATRIX_LAT_ROW_8 = 1; //send current to row
	MATRIX_LAT_ROW_8 = 0; //recind current to row
	
	MATRIX_LAT_COLOR_RED = ~0x00; //cutoff current into RED columns
	MATRIX_LAT_COLOR_GRN = ~0xFF; //sink current into GRN columns
	
	MATRIX_LAT_ROW_1 = 1; //send current to row
	MATRIX_LAT_ROW_1 = 0; //recind current to row
	MATRIX_LAT_ROW_2 = 1; //send current to row
	MATRIX_LAT_ROW_2 = 0; //recind current to row
	MATRIX_LAT_ROW_3 = 1; //send current to row
	MATRIX_LAT_ROW_3 = 0; //recind current to row
	MATRIX_LAT_ROW_4 = 1; //send current to row
	MATRIX_LAT_ROW_4 = 0; //recind current to row
	MATRIX_LAT_ROW_5 = 1; //send current to row
	MATRIX_LAT_ROW_5 = 0; //recind current to row
	MATRIX_LAT_ROW_6 = 1; //send current to row
	MATRIX_LAT_ROW_6 = 0; //recind current to row
	MATRIX_LAT_ROW_7 = 1; //send current to row
	MATRIX_LAT_ROW_7 = 0; //recind current to row
	MATRIX_LAT_ROW_8 = 1; //send current to row
	MATRIX_LAT_ROW_8 = 0; //recind current to row
	
	MATRIX_LAT_COLOR_GRN = ~0x00; //cutoff current into GRN columns
	MATRIX_LAT_COLOR_BLU = ~0xFF; //sink current into BLU columns
	
	MATRIX_LAT_ROW_1 = 1; //send current to row
	MATRIX_LAT_ROW_1 = 0; //recind current to row
	MATRIX_LAT_ROW_2 = 1; //send current to row
	MATRIX_LAT_ROW_2 = 0; //recind current to row
	MATRIX_LAT_ROW_3 = 1; //send current to row
	MATRIX_LAT_ROW_3 = 0; //recind current to row
	MATRIX_LAT_ROW_4 = 1; //send current to row
	MATRIX_LAT_ROW_4 = 0; //recind current to row
	MATRIX_LAT_ROW_5 = 1; //send current to row
	MATRIX_LAT_ROW_5 = 0; //recind current to row
	MATRIX_LAT_ROW_6 = 1; //send current to row
	MATRIX_LAT_ROW_6 = 0; //recind current to row
	MATRIX_LAT_ROW_7 = 1; //send current to row
	MATRIX_LAT_ROW_7 = 0; //recind current to row
	MATRIX_LAT_ROW_8 = 1; //send current to row
	MATRIX_LAT_ROW_8 = 0; //recind current to row
	
	MATRIX_LAT_COLOR_BLU = ~0x00; //cutoff current into BLU columns
}

void bitmapTest(void)
{
	unsigned char currentPixelPosition, currentPixelPwm;
	
	for(currentPixelPosition = 0; currentPixelPosition < MATRIX_COLUMNS; currentPixelPosition++)
	{
		bitmapDrawPixel(currentPixelPosition, 0, MATRIX_PWM_MAX, 0, 0);
	}
	
	for(currentPixelPosition = 0; currentPixelPosition < MATRIX_COLUMNS; currentPixelPosition++)
	{
		bitmapDrawPixel(currentPixelPosition, 1, 0, MATRIX_PWM_MAX, 0);
	}
	
	for(currentPixelPosition = 0; currentPixelPosition < MATRIX_COLUMNS; currentPixelPosition++)
	{
		bitmapDrawPixel(currentPixelPosition, 2, 0, 0, MATRIX_PWM_MAX);
	}
	
}

void bitmapDrawPixel(unsigned char x, unsigned char y, unsigned char r, unsigned char g, unsigned char b)
{
	unsigned char i, currentPixelPosition;
	
	i = currentPixelPosition = 0;
	
	while(i < y)
	{
		currentPixelPosition += (MATRIX_COLUMNS*MATRIX_COLORS); //24
		i++;
	}
	
	i = 0;
	while(i < x)
	{
		currentPixelPosition += MATRIX_COLORS; //3
		i++;
	}
	
	bitmap[currentPixelPosition++] = r;
	bitmap[currentPixelPosition++] = g;
	bitmap[currentPixelPosition] = b;
}

void bitmapGetPixel(unsigned char x, unsigned char y, unsigned char * r, unsigned char * g, unsigned char * b)
{
	unsigned char i, currentPixelPosition;
	
	i = currentPixelPosition = 0;
	
	while(i < y)
	{
		currentPixelPosition += (MATRIX_COLUMNS*MATRIX_COLORS); //24
		i++;
	}
	
	i = 0;
	while(i < x)
	{
		currentPixelPosition += MATRIX_COLORS; //3
		i++;
	}
	
	*r = bitmap[currentPixelPosition++];
	*g = bitmap[currentPixelPosition++];
	*b = bitmap[currentPixelPosition];
}

void bitmapDrawChar(signed char x, signed char y, unsigned char r, unsigned char g, unsigned char b, unsigned char c)
{	
	unsigned char bitmask, characterColumn, characterScanColumn, characterScanRow, characterScanColumnOffset, characterScanRowOffset;
	unsigned int index;
	
	if(x < 0) characterScanColumnOffset = abs(x);
	else characterScanColumnOffset = 0;
	if(y < 0) characterScanRowOffset = abs(y);
	else characterScanRowOffset = 0;
	
	index = (c*5) + characterScanColumnOffset;

	for(characterScanColumn = characterScanColumnOffset; characterScanColumn < (5 - characterScanColumnOffset); characterScanColumn++) //for use with 5x7 font
	{
		characterColumn = fontPtr[index++];
		bitmask = 0b00000001;
	
		for(characterScanRow = characterScanRowOffset; characterScanRow < (7 - characterScanRowOffset); characterScanRow++) //for use with 5x7 font
		{
			if((characterColumn & bitmask) > 0)
			{
				bitmapDrawPixel(characterScanColumn, characterScanRow, r, g, b);
			}
	
			bitmask <<= 1;
		}
	}
}

void bitmapClear(void)
{
	unsigned char i = 0;
	while(i < sizeof(bitmap))
	{
		bitmap[i++] = 0x00; //clear bitmap memory
	}
}

void setTime(void)
{
	unsigned char i, changesMadeFlag;
	signed int currentFieldValue, tempValue, tempValueTens, tempValueUnits;
	unsigned int currentFieldSelected;
	unsigned char tempHours, tempMinutes;
	
	currentFieldValue = currentFieldSelected = 0;
	changesMadeFlag = 1;
	
	//grab time
	tempHours = hours;
	tempMinutes = minutes;
	
	//turn on display refresh interrupt
	TMR0IE = 1;
	
	//turn on generic tick timer
	timerTicks = 0;
	TMR2 = 0;
	T2CONbits.TMR2ON = 1; //timer2 ON
	
	//scan keys
	while(1)
	{		
		if(timerTicks > 10000) break; //if enough one-second intervals have passed, exit time-set mode and discard changes
		
		if(SW1 == 0/*sw1IsPressed()*/) //if adj - key pressed, increment/decrement value and clamp value
		{
			__delay_ms(10); //debounce
			while(!SW1);
			__delay_ms(10); //debounce
			
			currentFieldValue--;
			changesMadeFlag = 1;
		}
		else if(SW2 == 0/*sw2IsPressed()*/) //if adj + key pressed, increment/decrement value and clamp value
		{
			__delay_ms(10); //debounce
			while(!SW2);
			__delay_ms(10); //debounce
			
			currentFieldValue++;
			changesMadeFlag = 1;
		}
		else if(SW3 == 0/*sw3IsPressed()*/) //if display button pressed, increment field.
		{
			__delay_ms(10); //debounce
			while(!SW3);
			__delay_ms(10); //debounce
			
			currentFieldSelected++;
			changesMadeFlag = 1;
		}
		
		if(changesMadeFlag) //apply changes
		{
			switch(currentFieldSelected)
			{	
				case 0: //hours field
					tempValue = tempHours;
					tempValue += currentFieldValue;
					
					//clamp to 23
					if(tempValue < 0) tempValue = 23;
					if(tempValue > 23) tempValue = 0;
					tempHours = tempValue;
				break;
				
				case 1: //minutes field
					tempValue = tempMinutes;
					tempValue += currentFieldValue;
					
					//clamp to 59
					if(tempValue < 0) tempValue = 59;
					if(tempValue > 59) tempValue = 0;
					tempMinutes = tempValue;
				break;
				
				default:
				break;
			}
			
			//set display digit, establish field selection
			//bitmapDrawChar((8-5), 0, 15, 15, 15, tempValue + '0');
			bitmapClear(); //TODO: just clear column area for efficiency	
			tempValueTens = (tempValue/10);
			tempValueUnits = (tempValue%10);
			
			for(i=0; i<tempValueTens; i++) //tens column
			{
					if(i < 8)
					{
						bitmapDrawPixel((currentFieldSelected<<2), i, 0, 0, MATRIX_PWM_MAX);
						bitmapDrawPixel((currentFieldSelected<<2)+1, i, 0, 0, MATRIX_PWM_MAX);
					}
					else
					{
						bitmapDrawPixel((currentFieldSelected<<2), 7, MATRIX_PWM_MAX, 0, 0);
						bitmapDrawPixel((currentFieldSelected<<2)+1, 7, MATRIX_PWM_MAX, 0, 0);
					}
			}
			
			for(i=0; i<tempValueUnits; i++) //units column
			{
					if(i < 8)
					{
						bitmapDrawPixel((currentFieldSelected<<2)+2, i, 0, 0, MATRIX_PWM_MAX);
						bitmapDrawPixel((currentFieldSelected<<2)+3, i, 0, 0, MATRIX_PWM_MAX);
					}
					else
					{
						bitmapDrawPixel((currentFieldSelected<<2)+2, 7, MATRIX_PWM_MAX, 0, 0);
						bitmapDrawPixel((currentFieldSelected<<2)+3, 7, MATRIX_PWM_MAX, 0, 0);
					}
			}
			
			//use something to show current field
			switch(currentFieldSelected)
			{	
				case 0:
					//set something within hour field

				break;

				case 1:
					//set something within hour minute field

				break;
				
				default:
				break;
			}	
			
			currentFieldValue = 0;
			timerTicks = 0; //reset inactivity timeout counter
			changesMadeFlag = 0;
		}
		
		if(currentFieldSelected > 1) //If all fields have been cycled through, save and exit time-set mode.
		{
			//TMR0IE = 0; //disable RTC timer interrupt while we adjust values
			hours = tempHours;
			minutes = tempMinutes;
			seconds = 0;
			//TMR0IE = 1; //re-enable RTC timer interrupt to keep counting time
			
			break;
		}
	}	
	
	//clear screen and turn off display refresh
	TMR0IE = 0;
	
	//turn off generic tick timer
	T2CONbits.TMR2ON = 0; //timer2 OFF
	timerTicks = 0;
	TMR2 = 0;
}

void getTime(void)
{
	if(newSecond)
	{
		newSecond--; // A second has accumulated, count it
		
		if(++seconds > 59)
		{
			seconds = 0;
			
			if(++minutes > 59)
			{
				minutes = 0;
				hours++;
				if(hours == 12) ampm ^= 1;
				if(hours>12) hours = 1;
			}
		}
	}
	
	#if ALARM == ON
		// If time matches alarm setting, toggle RC2
		if((hours == ALARM_H) && (minutes == ALARM_M) && (ampm == ALARM_AP) && (seconds < ALARM_LENGTH))
		{
			unsigned int tone;
			RC2 ^= 1;	// generate buzz
			tone = TONE2;
			if(seconds & 1) tone = TONE1;
			while(tone--) continue;	// tone generation
			RC2 ^= 1;	// generate buzz
		}
	#endif
}

void showTime(void)
{
	unsigned char currentAnimationFrame, currentPwmLevel, loopsUntilTimeout, \
	x, y, xLast, yLast, tempPixelR, tempPixelG, tempPixelB;
	signed char currentAnimationFrameSigned;
	unsigned char elementHourTens, elementHourUnits, \
	elementMinuteTens, elementMinuteUnits, \
	elementSecondTens, elementSecondUnits;
	
	currentAnimationFrame = currentPwmLevel = 0;
	
	getTime();
	
	elementHourTens = (hours/10);
	elementHourUnits = (hours%10);
	elementMinuteTens = (minutes/10);
	elementMinuteUnits = (minutes%10);
	
	//commence nifty animated time display
	//clear display
	bitmapClear();
	
	//turn on display refresh interrupt
	TMR0IE = 1;
	
	//TODO: read generic tick timer to process animation frames
	//TODO: configure generic timer for 50ms interval
	T2CONbits.TMR2ON = 1; //timer2 ON
	
	for(currentAnimationFrame = 0; currentAnimationFrame < 10; currentAnimationFrame++)
	{
		if(currentAnimationFrame < 8) //covering entire display for indicators 0-8
		{
			//TODO: advance hours (tens) bar
			if(elementHourTens != 0)
			{
				if(elementHourTens > currentAnimationFrame)
				{
					bitmapDrawPixel(0, currentAnimationFrame, 0, 0, MATRIX_PWM_MAX);
					bitmapDrawPixel(1, currentAnimationFrame, 0, 0, MATRIX_PWM_MAX);
				}
			}
			
			//TODO: advance hours (units) bar
			if(elementHourUnits != 0)
			{
				if(elementHourUnits > currentAnimationFrame)
				{
					bitmapDrawPixel(2, currentAnimationFrame, 0, 0, MATRIX_PWM_MAX);
					bitmapDrawPixel(3, currentAnimationFrame, 0, 0, MATRIX_PWM_MAX);
				}
			}
			
			//TODO: advance minutes (tens) bar
			if(elementMinuteTens != 0)
			{
				if(elementMinuteTens > currentAnimationFrame)
				{
					bitmapDrawPixel(4, currentAnimationFrame, 0, 0, MATRIX_PWM_MAX);
					bitmapDrawPixel(5, currentAnimationFrame, 0, 0, MATRIX_PWM_MAX);
				}
			}
			
			//TODO: advance minutes (units) bar
			if(elementMinuteUnits != 0)
			{
				if(elementMinuteUnits > currentAnimationFrame)
				{
					bitmapDrawPixel(6, currentAnimationFrame, 0, 0, MATRIX_PWM_MAX);
					bitmapDrawPixel(7, currentAnimationFrame, 0, 0, MATRIX_PWM_MAX);
				}
			}
		}
		else //backing up into display for indicator 9
		{
			//TODO: advance hours (tens) bar
			if(elementHourTens != 0)
			{
				if(elementHourTens > currentAnimationFrame)
				{
					bitmapDrawPixel(0, 7, MATRIX_PWM_MAX, 0, 0);
					bitmapDrawPixel(1, 7, MATRIX_PWM_MAX, 0, 0);
				}
			}	
			
			//TODO: advance hours (units) bar
			if(elementHourUnits != 0)
			{
				if(elementHourUnits > currentAnimationFrame)
				{
					bitmapDrawPixel(2, 7, MATRIX_PWM_MAX, 0, 0);
					bitmapDrawPixel(3, 7, MATRIX_PWM_MAX, 0, 0);
				}
			}
			
			//TODO: advance minutes (tens) bar
			if(elementMinuteTens != 0)
			{
				if(elementMinuteTens > currentAnimationFrame)
				{
					bitmapDrawPixel(4, 7, MATRIX_PWM_MAX, 0, 0);
					bitmapDrawPixel(5, 7, MATRIX_PWM_MAX, 0, 0);
				}
			}
			
			//TODO: advance minutes (units) bar
			if(elementMinuteUnits != 0)
			{
				if(elementMinuteUnits > currentAnimationFrame)
				{
					bitmapDrawPixel(6, 7, MATRIX_PWM_MAX, 0, 0);
					bitmapDrawPixel(7, 7, MATRIX_PWM_MAX, 0, 0);
				}
			}
		}
			
		timerTicks = 0; //reset framesync flag
		while(timerTicks < 25); //delay for effect
		//__delay_ms(10);
	}
	
	x = y = xLast = yLast = 0;
	
	for(loopsUntilTimeout = 0; loopsUntilTimeout < 4; loopsUntilTimeout++) //roughly 4 seconds
	{	
		x = (seconds/8);
		y = (seconds%8);

		if((xLast != x) || (yLast != y)) //if a second has advanced
		{	
			if(loopsUntilTimeout == 0) //preload tempPixel values
			{
				bitmapGetPixel(x, y, &tempPixelR, &tempPixelG, &tempPixelB);
			}
			else
			{
				bitmapDrawPixel(xLast, yLast, tempPixelR, tempPixelG, tempPixelB); //restore the last pixel
				bitmapGetPixel(x, y, &tempPixelR, &tempPixelG, &tempPixelB); //save the current pixel before altering it
			}
			
			xLast = x;
			yLast = y;
		}
		
		//display an out-fading dot corresponding to the current second
		for(currentAnimationFrameSigned = MATRIX_PWM_MAX; currentAnimationFrameSigned > -1; currentAnimationFrameSigned--)
		{
			bitmapDrawPixel(x, y, currentAnimationFrameSigned, currentAnimationFrameSigned, 0);
			timerTicks = 0;
			
			//idle about while user reads time, preferably in some power-saving mode
			while(timerTicks < 63); //delay for fading effect
		}
	}
	
	for(currentAnimationFrame = 1; currentAnimationFrame < (MATRIX_ROWS+1); currentAnimationFrame++)
	{
		//clear each row from the bottom
		for(x = 0; x < MATRIX_ROWS; x++)
		{
			bitmapDrawPixel(x, (MATRIX_ROWS - currentAnimationFrame), 0, 0, 0); 
		}
		
		timerTicks = 0; //reset framesync flag
		while(timerTicks < 25); //delay for effect
	}

	//turn off generic tick timer
	T2CONbits.TMR2ON = 0; //timer2 OFF
	TMR2 = 0;
	
	//turn off display refresh interrupt
	TMR0IE = 0;
	
	//ensure all pixels are off
	MATRIX_LAT_ROW_1 = MATRIX_LAT_ROW_2 = \
	MATRIX_LAT_ROW_3 = MATRIX_LAT_ROW_4 = \
	MATRIX_LAT_ROW_5 = MATRIX_LAT_ROW_6 = \
	MATRIX_LAT_ROW_7 = MATRIX_LAT_ROW_8 = 0;
}

/*
unsigned char sw1IsPressed(void)
{
    if(SW1 != old_sw1)
    {
	    __delay32((unsigned long)SW_DEBOUNCE); //10ms delay to debounce switch
	    
        old_sw1 = SW1;                  // Save new value
        if(SW1 == 0)                    // If pressed
            return 1;                // Was pressed
    }//end if
    return 0;                       // Was not pressed
}//end sw1IsPressed

unsigned char sw2IsPressed(void)
{
    if(SW2 != old_sw2)
    {
	    __delay32((unsigned long)SW_DEBOUNCE); //10ms delay to debounce switch
	    
        old_sw2 = SW2;                  // Save new value
        if(SW2 == 0)                    // If pressed
            return 1;                // Was pressed
    }//end if
    return 0;                       // Was not pressed
}//end sw1IsPressed

unsigned char sw3IsPressed(void)
{
    if(SW3 != old_sw3)
    {
	    __delay32((unsigned long)SW_DEBOUNCE); //10ms delay to debounce switch
	    
        old_sw3 = SW3;                  // Save new value
        if(SW3 == 0)                    // If pressed
            return 1;                // Was pressed
    }//end if
    return 0;                       // Was not pressed
}//end sw1IsPressed
*/

unsigned char getBattery(void)
{
	//charge up FVR to 2.048v. FVR takes 35uA. FVR is always ready for non-LF parts!
	FVRCONbits.FVREN = 1; //turn on FVR
	while(!FVRCONbits.FVRRDY); //wait for FVR to stabilize
	
	ADCON0bits.ADON = 1; //turn on ADC
	
	BATT_VSENSE_GND_TRIS = 0; //sink bottom of VSENSE divider
	
	//TODO: wait acquisition time
	
	ADCON0bits.GO_nDONE = 1; //initiate conversion (sample battery voltage against FVR)
	while(ADCON0bits.GO_nDONE); //wait for conversion to finish
	 
	FVRCONbits.FVREN = 0; //turn off FVR
	ADCON0bits.ADON = 0; //turn off ADC
	BATT_VSENSE_GND_TRIS = 1; //tristate bottom of VSENSE divider
	
	return ADRESH;
}
