#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "LED.h"

// R,G,B pinouts: PF3, PB3, PC4
#define RED_DATA (*((volatile uint32_t *)0x40025030))
#define GREEN_DATA (*((volatile uint32_t *)0x40005030))
#define BLUE_DATA (*((volatile uint32_t *)0x40006040))

void BSP_LED_init(void) {
	SYSCTL_RCGCGPIO_R |= 0x00000026;					// activate clock for Port B, C, and F
	while((SYSCTL_PRGPIO_R&0x26) != 0x26){};	// allow time for clocks to stabilize
	GPIO_PORTF_LOCK_R = 0x4C4F434B;						// unlock GPIO Port F
	GPIO_PORTF_CR_R = 0x1F;										// allow changes to PF4-0
	GPIO_PORTF_AMSEL_R &= ~0x08;							// disable analog on PF3
	GPIO_PORTB_AMSEL_R &= ~0x08;							// disable analog on PB3
	GPIO_PORTC_AMSEL_R &= ~0x10;							// disable analog on PC4
	GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFF0FFF)+0x00000000;		// configure PF3 as GPIO
	GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFFF0FFF)+0x00000000;		// configure PB3 as GPIO
	GPIO_PORTC_PCTL_R = (GPIO_PORTC_PCTL_R&0xFFF0FFFF)+0x00000000;		// configure PC4 as GPIO
	GPIO_PORTF_DIR_R |= 0x08;									// make PF3 output
	GPIO_PORTB_DIR_R |= 0x08;									// make PB3 output
	GPIO_PORTC_DIR_R |= 0x10;									// make PC4 output
	GPIO_PORTF_AFSEL_R &= ~0x08;							// disable alt funct on PF3
	GPIO_PORTB_AFSEL_R &= ~0x08;							// disable alt funct on PB3
	GPIO_PORTC_AFSEL_R &= ~0x10;							// disable alt funct on PC4
	GPIO_PORTF_DEN_R |= 0x08;									// enable digital I/O on PF3
	GPIO_PORTB_DEN_R |= 0x08;									// enable digital I/O on PB3
	GPIO_PORTC_DEN_R |= 0x10;									// enable digital I/O on PC4
}

void BSP_LED_on(void) {
	RED_DATA = 0xFF;
	GREEN_DATA = 0xFF;
	BLUE_DATA = 0xFF;
}

void BSP_LED_off(void) {
	RED_DATA = 0x00;
	GREEN_DATA = 0x00;
	BLUE_DATA = 0x00;
}

void BSP_LED_setGreen(void) {
	RED_DATA = 0x00;
	GREEN_DATA = 0xFF;
	BLUE_DATA = 0x00;
}

void BSP_LED_setYellow(void) {
	RED_DATA = 0xFF;
	GREEN_DATA = 0xFF;
	BLUE_DATA = 0x00;
}

void BSP_LED_setRed(void) {
	RED_DATA = 0xFF;
	GREEN_DATA = 0x00;
	BLUE_DATA = 0x00;
}
