/*
 * AVRProgrammingTask.c
 *
 * Created: 26/10/2020 10:22:10 AM
 * Author : william.barker
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
// checking if extended wash mode is selected and no error is present
#define EXTENDED (((PIND & (1 << PIND4)) == (1 << PIND4)) &&  (PIND & 3) != 3)
// checking if normal wash mode is selected and no error is present
#define NORMAL (((PIND & (1 << PIND4)) == 0 && (PIND & 3) != 3))

// Seven segment display values for water level/ mode select.
uint8_t seven_seg[5] = {8, 1, 64, 121, 84};
// Pulse Width Modulation values for OCR0B at 10%, 50% and 90%  duty cycle respectively.
uint8_t pwm[3] = {230, 128, 26};
	
/* 0 = right display, 1 = left display */
volatile uint8_t digit;
/* index for seven_seg array */
volatile uint8_t indexNumber;
/* counts time for LED patterns during wash/rinse/spin cycle */
volatile uint8_t timeCounter;
/* int that stores whether system has finished or not */
volatile uint8_t finished;

/* Display function. Arguments are the index of seven_seg display array
 * and the digit to display it on (0 = right, 1 = left). The function 
 * outputs the correct seven segment display value and digit select to PORTA. 
 * If a wash cycle is finished, zero is displayed on both displays.
 */
void display(uint8_t indexNumber, uint8_t digit, uint8_t finished) {
	if (finished == 0) {
		PORTA = (seven_seg[indexNumber] & 0x7F) | (digit << 7);	
	} else {
		PORTA = 63 | (digit << 7);
	}
}

/* washCycle function. Argument is the time Counter value. 
 * Function determines the appropriate PORTC output to turn 
 * on the correct LED in the wash cycle pattern.
*/
uint8_t washCycle(uint8_t timeCounter) {
	if ((timeCounter / 2) % 16 < 8) {
		return (1 << ((timeCounter / 2) % 4)); // pattern from L0 - L3 (changes on every second compare match)
		} else {
		return 0b00001111; // turn on all LED's
	}
}

/* rinseCycle function. Argument is the time Counter value. 
 * Function determines the appropriate PORTC output to turn 
 * on the correct LED in the rinse cycle pattern.
*/
uint8_t rinseCycle(uint8_t timeCounter) {
	if ((timeCounter/2) % 16 < 8) {
		return (1 << (3-((timeCounter/2) % 4))); // pattern from L3 - L0 (changes on every second compare match)
		} else if (timeCounter % 4 < 2) {
		return 0b00001111;  // all LED's on for two compare matches
		} else {
		return 0b00000000; // all LED's off for two compare matches
	}
}

/* spinCycle function. Argument is the time Counter value. 
 * Function determines the appropriate PORTC output to turn 
 * on the correct LED in the spin cycle pattern.
*/
uint8_t spinCycle(uint8_t timeCounter) {
	if ((timeCounter/2) % 16 < 4) {
		return (1 << ((timeCounter/2) % 4));  // pattern from L0 - L3 (changes on every second compare match)
		} else if ((timeCounter/2) % 16 < 8) {
		return (1 << (3 - ((timeCounter/2) % 4))); // pattern from L3 - L0 (changes on every second compare match)
		} else if (timeCounter % 2 == 0) {
		return 0b00001111;  // all LED's on for one compare matches
		} else {
		return 0b00000000; // all LED's off for one compare matches
	}
}

/* reset function. This function is used to reset 
 * to default settings after a wash is complete or 
 * the reset button is pressed
*/
void reset() {
	timeCounter = 0; // reset timer counter to 0
	TIMSK1 = (0 << OCIE1A); // disable timer 1 interrupt
	TIFR1 = (1 << OCF1A); // clear timer 1 interrupt flag
	OCR0B = 255; // turn off PWM controlled LED
	PORTC = 0; // turn off LED's
	TCCR1B =  (0 << CS12) | (0 << CS11) | (0 <<CS10); // Turn clock off
	EIMSK = (1 << INT0) | (1 << INT1); // turn on B0 and B1 interrupts
	EIFR = (1 << INTF0) | (1 << INTF1); // clearing interrupt flags
}

/* startSystem function. This function is used to start the 
 * LED pattern when B0 is pressed.
 */
void startSystem() {
			timeCounter = 0; // reset timer counter to 0
			PORTC = 1; // turn on LED 0 on the IO Board
			OCR0B = pwm[0]; // turn on 10% duty cycle PWM
			TCCR1B = (0 << WGM13) | (1 << WGM12) | (1 << CS12) | (0 << CS11) | (0 <<CS10); // turning on clock with prescaler of 256
			TIMSK1 = (1 << OCIE1A); // turning on interrupt for clock 1
			TIFR1 = (1 << OCF1A); // clear interrupt flag
			EIMSK = (0 << INT0) | (1 << INT1); // turn off interrupt associated with B0 while ensuring B1 interrupt is still on
			EIFR = (1 <<INTF0) | (1 << INTF1); // clear interrupt flags
}

int main(void) {
	/* Set port A (all pins) to be outputs */
	DDRA = 0xFF;
	/* Set first 4 pins of PORTC to outputs */
	DDRC = 0xFF & 0b00001111;
	/* Set port B, pin 4 to be an output */
	DDRB = (1 << PORTB4);
	/* Set all pins on PortD to be inputs */
	DDRD = 0;

	/* Initializing appropriate settings for Fast PWM
	WGM02 = 0 & WGM01 = 1 & WGM00 = 1  -> Fast  PWM mode
	COM0B1 = 1 & COM0B0 = 1 -> Set on compare match, clear on bottom
	CS02 = 0 & CS01 = 0 & CS00 = 1  -> clock with no prescaler
	OCR0B = 255  -> OC0B pin is off 
	*/
	OCR0B = 255;
	TCCR0A = (1<<COM0B1) | (1<<COM0B0) | (1<<WGM01) | (1<<WGM00);
	TCCR0B = (0<<WGM02) | (0<<CS02) | (0<<CS01) | (1<<CS00);
		
	/* Initializing timer to appropriate settings
	 * WGM13 = 0 & WGM12 = 1  -> CTC mode
	 * CS12 = 0 & CS11 = 0 & CS10 0  -> Clock Off
	 * OCR1A = 5858  -> appropriate value so that when the clock 
	 * is set to a prescaler of 256 it will cycle 
	 * 16 times every 3 seconds.
	*/
	OCR1A = 5858;  
	TCCR1A = 0;  
	TCCR1B = (0 << WGM13) | (1 << WGM12) | (0 << CS12) | (0 << CS11) | (0 <<CS10); 
	
	OCR2A = 255;
	TCCR2A = (0 << COM1A1) | (1 << COM1A0)  // Toggle OC1A on compare match
		| (0 << WGM11) | (0 << WGM10); // Least two significant WGM bits
	TCCR2B = (0 << WGM13) | (1 << WGM12) // Two most significant WGM bits
		| (0 << CS12) | (0 << CS11) | (1 <<CS10); // Turn clock on
	
	
	/* Set up interrupts to occur on rising edge of pin D2 (start button) and D3 (reset button) */
	EICRA = (1 << ISC01)|(0 << ISC00) | (1 << ISC11)|(0 << ISC10);
	EIMSK = (1 << INT0) | (1 << INT1);
	EIFR = (1 << INTF0) | (1 << INTF1);
	
	/* Turn on global interrupts */
	sei();

	// Initializing variables to there respective starting states
	finished = 0;
	digit = 0;
	while(1) {
		/* sets value for seven_seg index to the appropriate water level output index 
		 * (if right display is selected) or the appropriate mode select index
		 * (if left display is selected).
		*/
		if (digit == 0) {
			indexNumber = PIND & 0x3;
		} else {
			if ((PIND & 16) == 16) {
				indexNumber = 3;
			} else {
				indexNumber = 4;
			}
		}
		/* display the appropriate value on seven-segment display */
		display(indexNumber, digit, finished);
		/* Change the digit flag for next time. if 0 becomes 1, if 1 becomes 0. */
		digit = 1 - digit;

		/* Wait for timer 1 to reach output compare A value.
		 * We can monitor the OCF1A bit in the TIFR1 register. When 
		 * it becomes 1, we know that the output compare value has
		 * been reached. We can write a 1 to this bit to clear it.
		 */
		while ((TIFR2 & (1 << OCF2A)) == 0) {
		 	; /* Do nothing - wait for the bit to be set */
		}
		/* Clear the output compare flag - by writing a 1 to it. */
		TIFR2 &= (1 << OCF2A);
	}
}

ISR(INT0_vect) {
	/* checking if extended or normal mode conditions are
	 * met and if so system cycle is started. 
	 */
	if (EXTENDED || NORMAL) {
		startSystem();
	}
	// checking if system is finished
	if (finished == 1) {
		finished = 0; // resets system finished variable if the system is restarted
	}
}

ISR(INT1_vect) {
	reset(); // reseting system if B1 is pressed
	finished = 0; // indicates cycle is not finished
}

ISR(TIMER1_COMPA_vect) {
	if (EXTENDED) { 
		// add 1 to counter every time clock counter restarts.
		timeCounter += 1;
		/* timer counter is increased 32 times every 6 seconds. 
		 * Hence each cycle is 32 long.
		 */
		if (timeCounter < 32) {
			PORTC = washCycle(timeCounter);
		} else if (timeCounter < 96) {
			OCR0B = pwm[1]; // changes PWM values to 50%
			PORTC = rinseCycle(timeCounter);
		} else if (timeCounter < 128) {
			OCR0B = pwm[2]; // changes PWM values to 90%
			PORTC = spinCycle(timeCounter);
		} else {
			reset();
			finished = 1; // indicates system has finished
		}
	} else if (NORMAL) {
		// add 1 to counter every time clock counter restarts.
		timeCounter += 1;
		/* timer counter is increased 32 times every 6 seconds. 
		 * Hence each cycle is 32 long.
		 */
		if (timeCounter < 32) {
			PORTC = washCycle(timeCounter);
		} else if (timeCounter < 64) {
			OCR0B = pwm[1]; // changes PWM values to 50%
			PORTC = rinseCycle(timeCounter);
		} else if (timeCounter < 96) {
			OCR0B = pwm[2]; // changes PWM values to 90%
			PORTC = spinCycle(timeCounter);
		} else {
			reset();
			finished = 1; // indicates system has finished
		}	
	}
}
