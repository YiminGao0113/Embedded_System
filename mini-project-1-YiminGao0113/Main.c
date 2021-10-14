#include <stdint.h>
#include "PLL.h"
#include "LCD.h"
#include "os.h"
#include "joystick.h"
#include "FIFO.h"
#include "PORTE.h"
#include "tm4c123gh6pm.h"

// Constants
#define BGCOLOR     					LCD_BLACK
#define CROSSSIZE            			5

//------------------Defines and Variables-------------------
uint16_t origin[2]; // the original ADC value of x,y if the joystick is not touched
int16_t x = 63;  // horizontal position of the crosshair, initially 63
int16_t y = 63;  // vertical position of the crosshair, initially 63
int16_t prevx = 63;
int16_t	prevy = 63;
uint8_t select;  // joystick push

//---------------------User debugging-----------------------

#define TEST_TIMER 0		// Change to 1 if testing the timer
#define TEST_PERIOD 4000000  // Defined by user
#define PERIOD 4000000  		// Defined by user

unsigned long Count;   		// number of times thread loops


//--------------------------------------------------------------
void CrossHair_Init(void){
	BSP_LCD_FillScreen(LCD_BLACK);	// Draw a black screen
	BSP_Joystick_Input(&origin[0], &origin[1], &select); // Initial values of the joystick, used as reference
}

//******** Producer *************** 
void Producer(void){
#if TEST_TIMER
	PE1 ^= 0x02;	// heartbeat
	Count++;	// Increment dummy variable			
#else
	// Variable to hold updated x and y values
	int16_t newX = x;
	int16_t newY = y;
	int16_t deltaX = 0;
	int16_t deltaY = 0;
	
	uint16_t rawX, rawY; // To hold raw adc values
	uint8_t select;	// To hold pushbutton status
	rxDataType data;
	BSP_Joystick_Input(&rawX, &rawY, &select);
	
	// Your Code Here
	if (select){              // Add an additional reset function triggerred by select
		if(rawX > 2500)
			newX += 2;
		else if(rawX < 1000)
			newX -= 2;

		if(rawY > 2500)
			newY -= 2;
		else if(rawY < 1000)
			newY += 2;

		if(newX > 127)        // Set the range
			newX = 127;
		else if(newX < 0)
			newX = 0;

		if(newY > 127)
			newY = 127;
		else if(newY < 0)
			newY = 0;
	}else{                  // When the pushbutton is triggerred, reset the crosshair location (63,63)
		newX = 63;            
		newY = 63;
	}

	data.x = newX;
	data.y = newY;

	RxFifo_Put(data);      // Put the new data to fifo
	
#endif
}

//******** Consumer *************** 
void Consumer(void){
	rxDataType data;
	// Your Code Here
	char xStr[] = "X: ";
	char yStr[] = "Y: ";

	prevx = data.x;           // Store the previous data before RxFifo_Get
	prevy = data.y;

	RxFifo_Get(&data);        // Get the new location from fifo

	x = data.x;               // Update the address
	y = data.y;

	BSP_LCD_DrawCrosshair(prevx, prevy, LCD_BLACK);     // Set the previous crosshair to black
	BSP_LCD_DrawCrosshair(x, y, LCD_WHITE);             // Draw the crosshair with updated location
	BSP_LCD_Message(0, 12, 4, xStr, x);                 // Print the location information
	BSP_LCD_Message(0, 12, 12, yStr, y);

}

//******** Main *************** 
int main(void){
  PLL_Init(Bus80MHz);       // set system clock to 80 MHz
#if TEST_TIMER
	PortE_Init();       // profile user threads
	Count = 0;
	OS_AddPeriodicThread(&Producer, TEST_PERIOD, 1);
	while(1){}
#else
  	BSP_LCD_Init();        // initialize LCD
	BSP_Joystick_Init();   // initialize Joystick
  	CrossHair_Init();      
 	RxFifo_Init();
	OS_AddPeriodicThread(&Producer,PERIOD, 1);
	while(1){
		Consumer();
	}
#endif
} 
