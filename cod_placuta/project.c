/*
 * Bogdan-Cristian Firuti 331CA
 */

#include "lcd.h"
#include "usart.h"
#include "dht22.h"
#include "adc.h"
#include "DS3231.h"

#include <avr/io.h>
#include <util/delay.h>	
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REF 170
#define THRESH 512

// variables used
static char list[10][80];
static char notification[80];
static char output_t[20];
static int info_counter = 0;
static int size = 0;
static int list_index = 0;
static int change = 0;
static int shown_notif = 0;
static int all_notif = 0;
static int last_bpm = 0;
static int last_h = 0;
static int last_m = 0;
static int alarm_set = 0;
static int alarm_on = 0;
static int option = 0;
static uint8_t alarm_hour;
static uint8_t alarm_min;

int pulsePin = 0;             			// Pulse Sensor connected to pin A0
volatile int rate[10];                    	// array to hold last ten IBI values
volatile unsigned long sampleCounter = 0;  	// used to determine pulse timing
volatile unsigned long lastBeatTime = 0;    	// used to find IBI
volatile int P = THRESH;                      	// used to find peak in pulse wave, seeded
volatile int T = THRESH;                     	// used to find trough in pulse wave, seeded
volatile int thresh = THRESH;                	// used to find instant moment of heart beat, seeded
volatile int amp = 100;                   	// used to hold amplitude of pulse waveform, seeded
volatile bool firstBeat = true;     		// used to seed rate array so we startup with reasonable BPM
volatile bool secondBeat = false;   		// used to seed rate array so we startup with reasonable BPM
volatile int BPM;                   		// int that holds raw Analog in 0. updated every 2mS
volatile int Signal;                		// holds the incoming raw data
volatile int IBI = 600;             		// holds the time interval between beats! Must be seeded!
volatile bool Pulse = false;     	// "True" when User's live heartbeat is detected. "False" when not a "live beat"
volatile bool QS = false;        	// becomes true when a beat is detected

// read info from pulse sensor at an interrupt
ISR(TIMER1_COMPA_vect){   
	uint8_t sec, min, h, m, d, y, s;                                                         
	cli();				// disable interrupts while we do this
	Signal = ADC_get(pulsePin);	// read signal from the Pulse Sensor  
	sampleCounter += 2;		// keep track of the time in mS
	int N = sampleCounter - lastBeatTime;	// monitor the time since the last beat to avoid noise

	// find the peak and trough of the pulse wave
	if(Signal < thresh && N > (IBI / 5) * 3) {   // avoid dichrotic noise by waiting 3/5 of last IBI
		if (Signal < T) {	// T is the trough
			T = Signal;	// keep track of lowest point in pulse wave
		}
	}

	if(Signal > thresh && Signal > P) {	// thresh condition helps avoid noise
		P = Signal;			// P is the peak
	}					// keep track of highest point in pulse wave

	// look for the heartbeats
	if (N > 250) {			// wait 250 ms to avoid high frequency noise
		if ((Signal > thresh) && (Pulse == false) && (N > (IBI / 5) * 3)) {
			Pulse = true;	// set the Pulse flag when we think there is a pulse
	    		IBI = sampleCounter - lastBeatTime;	// measure time between beats in mS
	    		lastBeatTime = sampleCounter;		// keep track of time for next pulse
	   		if(secondBeat) {			// if this is the second beat
	        		secondBeat = false;		// clear secondBeat flag
	       			for(int i = 0; i <= 9; i++) {	// seed the running total to get a realisitic BPM at startup
					rate[i] = IBI;
				}
			}
			
			if(firstBeat) {			// if it's the first time we found a beat
				firstBeat = false;	// clear firstBeat flag
				secondBeat = true;	// set the second beat flag
				sei();			// enable interrupts again
				return;			// IBI value is unreliable so discard it
			}

			// collect the last 10 IBI values
			int runningTotal = 0;		// clear the runningTotal variable
  		
			for(int i = 0; i <= 8; i++) {		// shift data in the rate array
				rate[i] = rate[i + 1];		// and drop the oldest IBI value
				runningTotal += rate[i];	// add up the 9 oldest IBI values
			}

			rate[9] = IBI;				// add the latest IBI to the rate array
			runningTotal += rate[9];		// add the latest IBI to runningTotal
			runningTotal /= 10;			// average the last 10 IBI values

			// BPM = how many beats can fit into a minute
			BPM = 60000/runningTotal;
			last_bpm = BPM * 100 / REF; // not so accurate => scale it
			DS3231_getTime(&sec, &min, &h, &s, &d, &m, &y);
			last_h = h;
			last_m = min;
			
			QS = true;	// set Quantified Self flag
		}
	}

	if (Signal < thresh && Pulse == true) {	// when the values are going down, the beat is over                  
		Pulse = false;			// reset the Pulse flag so we can do it again
		amp = P - T;			// get amplitude of the pulse wave
		thresh = amp/2 + T;		// set thresh at 50% of the amplitude
		P = thresh;			// reset these for next time
		T = thresh;
	}

	if (N > 2500) {				// if 2.5 seconds go by without a beat
		thresh = THRESH;			// set thresh default
		P = THRESH;			// set P default
		T = THRESH;			// set T default
		lastBeatTime = sampleCounter;	// bring the lastBeatTime up to date
		firstBeat = true;		// set these to avoid noise
		secondBeat = false;		// when we get the heartbeat back
	}

	sei();		// enable interrupts when youre done!                         
}

// Init of timer 1
void timer1_init()
{
	// CTC mode with top at OCR1A = 124, 0x7C, value for 2ms, PS = 256
	TCCR1A = 0x00;
	TCCR1B = 0x0C; 
	OCR1A = 0x7C; 
	TIMSK1 = 0x02;  // enable interrupt for comparision with OCR1A

	sei(); // enable global interrupts
}

// Bluetooth receiver interrupt
ISR(USART0_RX_vect) {
	// only one character received
	char c = USART0_receive();
	char output[20];
	char tmp[20];
	char d[2];
	char ok = 1;
	char *p;
	int i = 0;
	int sz;
	int app_found;
	int t, h;
	d[0] = c;
	d[1] = 0;

	// try to form the message (ex. {sync}, {scroll}, etc)
	if (c == '{') {
		// beginning of message
		memset(notification, 0, 80);
		size = 0;
	} else if (c == '}') {
		// end of message
		if (strncmp(notification, "scroll", 6) == 0) {
			// show next notification from list
			shown_notif = (shown_notif + 1) % all_notif;
		} else if (strncmp(notification, "change", 6) == 0) {
			// change to next screen
			change = 1;
		} else if (strncmp(notification, "reset", 5) == 0) {
			// reset display
			strcpy(list[0], "ALARM STOPPED");

			// stop alarm
			PORTB |= (1 << PB1);
			alarm_set = 0;
			alarm_on = 0;
		} else if (strncmp(notification, "alarm", 5) == 0) {
			char *token = strtok(notification, ":");

			// take hour and min and set alarm
			token = strtok(NULL, ":");
			alarm_hour = atoi(token);

			token = strtok(NULL, ":");
			alarm_min = atoi(token);

			alarm_set = 1;
		} else if (strncmp(notification, "sync", 4) == 0) {
			// send data to phone
			dhtxxread( DHTXX_DHT22, &PORTB, &DDRB, &PINB, ( 1 << PB0 ), &t, &h );
			strcpy(list[0], "GOT SYNC");
			memset(output, 0, 20);
			strcpy(output, "{");

			memset(tmp, 0, 20);
			itoa(t, tmp, 10);
			strcat(output, tmp);

			strcat(output, ",");

			memset(tmp, 0, 20);
			itoa(h, tmp, 10);
			strcat(output, tmp);

			strcat(output, "}");

			USART0_print(output); 	
		} else {
			// check for duplicates
			p = strchr(notification, '-');
			app_found = -1;
			for (i = 0; i < list_index; ++i) {
				if (strcmp(list[i], notification) == 0) {
					ok = 0;
					break;
				}

				if (p != NULL) {
					sz = p - notification;
					if (strncmp(list[i], notification, sz) == 0) {
						app_found = i;
					}
				}
			}

			
			if (ok && app_found == -1 && strlen(notification) > 1) {
				// new notification, add to the list
				memcpy(list[list_index], notification, 80);
				list_index = (list_index + 1) % 10;
				all_notif++;
				all_notif = (all_notif > 10) ? 10 : all_notif;

				// sounds when I get a new notification
				PORTB &= ~(1 << PB1);
				_delay_ms(500);
				PORTB |= (1 << PB1);
			} else if (ok && app_found != -1 && strlen(notification) > 1) {
				// notification from same app => keep the last one
				memcpy(list[app_found], notification, 80);

				// sounds when I get a new notification
				PORTB &= ~(1 << PB1);
				_delay_ms(500);
				PORTB |= (1 << PB1);
			}
		}
	} else {
		// continue reading characters
		if (size < 79 && c != 0) {
			strcat(notification, d);
			size++;
		}		
	}
}

// we have two buttons
ISR(PCINT3_vect) {
	// make transition to the next screen
	if ((PIND & (1 << PD4)) == 0) {
		change = 1;
	} else if ((PIND & (1 << PD5)) == 0) {
		// deactivate alarm or show next notification
		if (alarm_on == 0) {
			shown_notif = (shown_notif + 1) % all_notif;
		} else {
			PORTB |= (1 << PB1);
			alarm_set = 0;
			alarm_on = 0;
		}
	}
}

void check_alarm() {
	uint8_t sec, m, d, y, s, min, h;
	char buf[30];
	
	// check if alarm should be on
	DS3231_getTime(&sec, &min, &h, &s, &d, &m, &y);
	if (alarm_set == 1) {
		sprintf(buf, "ALARM AT %d:%d", alarm_hour, alarm_min);
		strcpy(list[0], buf);	
	}

	if ((alarm_set == 1) && (alarm_hour == h) && (alarm_min == min)) {
		PORTB &= ~(1 << PB1);
		alarm_on = 1;
	} else if ((alarm_hour == h) && (alarm_min + 1 <= min)) {
		PORTB |= (1 << PB1);
		alarm_set = 0;
		alarm_on = 0;
	}
}

void show_notifications(char *indexes, char *max_indexes) {
	int i;
	// reset positions (if notification is bigger than 16, it should
	// go round the screen and be all visible
	for (i = 0; i < 10; ++i) {
		max_indexes[i] = strlen(list[i]) - 16;
		if (max_indexes[i] < 0) {
			max_indexes[i] = 0;
		}
	}

	LCD_writeInstr(LCD_INSTR_clearDisplay);
	LCD_print(list[shown_notif] + indexes[shown_notif]);
	LCD_writeInstr(LCD_INSTR_nextLine);
	LCD_print(list[shown_notif + 1] + indexes[shown_notif + 1]);
	_delay_ms(750);

	// show next characters from the notifications
	for (i = 0; i < 10; ++i) {
		indexes[i]++;
		if (indexes[i] > max_indexes[i]) {
			indexes[i] = 0;
		}
	}
}

void show_info() {
	char output_h[20];
	char temp_str[5];
	char temp_str_1[5];
	uint8_t sec, min, h, m, d, y, s;
	int temp, temp1, humid, humid1;

	info_counter = (info_counter + 1) % 5; // read data once at 5 seconds
	if (info_counter == 0) {
		//Read data from sensor to variables `temp` and `humid` (`ec` is exit code)
		dhtxxread( DHTXX_DHT22, &PORTB, &DDRB, &PINB, ( 1 << PB0 ), &temp, &humid );

		temp1 = temp % 10;
		temp /= 10;

		humid1 = humid % 10;
		humid /= 10;
		

		// Temperature
		memset(temp_str, 0, 5);
		memset(temp_str_1, 0, 5);
		memset(output_t, 0, 20);

		itoa(temp, temp_str, 10);
		itoa(temp1, temp_str_1, 10);

		strcpy(output_t, "T:");
		strcat(output_t, temp_str);
		strcat(output_t, ".");
		strcat(output_t, temp_str_1);
		strcat(output_t, "C");

		// Humidity
		memset(temp_str, 0, 5);
		memset(temp_str_1, 0, 5);

		itoa(humid, temp_str, 10);
		itoa(humid1, temp_str_1, 10);

		strcat(output_t, " H:");
		strcat(output_t, temp_str);
		strcat(output_t, ".");
		strcat(output_t, temp_str_1);
		strcat(output_t, "%");
	}

	// display hour, minute and seconds
	DS3231_getTime(&sec, &min, &h, &s, &d, &m, &y);
	memset(output_h, 0, 20);
	sprintf(output_h, "Hour: %d:%d:%d", h,min,sec);

	LCD_writeInstr(LCD_INSTR_clearDisplay);
	LCD_print(output_t);

	LCD_writeInstr(LCD_INSTR_nextLine);
	LCD_print(output_h);
	_delay_ms(1000);
}

void show_bpm() {
	int i;
	int value = BPM * 100 / REF;	
	char buf[20];
	char additional[20];

	sprintf(buf, "BPM: ");
	sprintf(additional, "Last:%d %d:%d", last_bpm, last_h, last_m);
	
	if (QS == true && value > 60) {     // A Heartbeat was found, BPM has been determined
		itoa(value, buf, 10);
		sprintf(buf, "BPM: %d", value);
		
        	QS = false;    // reset the Quantified Self flag for next time
		for (i = 0; i < 9; ++i) {
			rate[i] = IBI;
		}
	}
	LCD_writeInstr(LCD_INSTR_clearDisplay);
	LCD_print(buf);
	LCD_writeInstr(LCD_INSTR_nextLine);
	LCD_print(additional);

	// change from 10 to 10 seconds
	_delay_ms(10000);
}

void init() {
	// initialize ports
	DDRD |= (1 << PD7);
	DDRD &= ~(1 << PD4);
	DDRD &= ~(1 << PD5);
	DDRB |= (1 << PB1);
	PORTD |= (1 << PD4);
	PORTD |= (1 << PD5);
	PORTB |= (1 << PB1);

	PCICR |= (1 << PCIE3);
	PCMSK3 |= (1 << PCINT30) | (1 << PCINT29) | (1 << PCINT28);
	sei(); // set interrupts on

	USART0_init();
   	LCD_init();
	twi_INIT();
	// DS3231_setTime(0, 41, 16, 4, 2, 5, 2019); // only the first time
	dhtxxconvert(DHTXX_DHT22, &PORTB, &DDRB, &PINB, (1 << PB0));
	ADC_init();
	timer1_init();
	_delay_ms(1000);
}

int main() {
	char indexes[10];
	char max_indexes[10];
	int i;
	
	init();

	// reset indexes
	for (i = 0; i < 10; ++i) {
		indexes[i] = 0;
	}

	while(1) {
		// see what we have to do
		check_alarm();
		if (change) {
			option = (option + 1) % 3;
			change = 0;
		}
		switch (option) {
			case 0:	show_notifications(indexes, max_indexes);
				break;
			case 1: show_info();
				break;
			case 2: show_bpm();
				break;
		}
	}

    return 0;
}

