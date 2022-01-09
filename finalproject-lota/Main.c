 // Main.c
// Runs on LM4F120/TM4C123
// You may use, edit, run or distribute this file 
// You are free to change the syntax/organization of this file

// Jonathan W. Valvano 2/20/17, valvano@mail.utexas.edu
// Modified by Sile Shu 10/4/17, ss5de@virginia.edu
// Modified by Mustafa Hotaki 7/29/18, mkh3cf@virginia.edu

#include <stdint.h>
#include "OS.h"
#include "tm4c123gh6pm.h"
#include "LCD.h"
#include <string.h> 
#include "UART.h"
#include "FIFO.h"
#include "joystick.h"
#include "PORTE.h"
#include "LED.h"

// Constants
#define BGCOLOR     					LCD_BLACK
#define CROSSSIZE            	5
#define PERIOD               	4000000   // DAS 20Hz sampling period in system time units
#define LED_BLINK_PERIOD      20000000
#define PSEUDOPERIOD         	8000000
#define LIFETIME             	1000
#define RUNLENGTH            	6000 
#define POLY_MASK_32          0xB4BCD35C
#define POLY_MASK_31          0x7A5BC2E3

extern Sema4Type LCDFree;
uint16_t origin[2]; 	// The original ADC value of x,y if the joystick is not touched, used as reference
int16_t x = 63;  			// horizontal position of the crosshair, initially 63
int16_t y = 63;  			// vertical position of the crosshair, initially 63
int16_t prevx, prevy;	// Previous x and y values of the crosshair
int16_t prevx, prevy;	// Previous x and y values of the crosshair
int16_t bulletx, bullety, bulletv;
uint8_t select;  			// joystick push
uint8_t area[2];
uint32_t PseudoCount;
uint32_t score = 0;
uint32_t life = 5;
uint8_t LedOn = 0;
uint8_t restartCount = 0;


typedef struct {
uint32_t position[2];
uint32_t available;
} block;
block BlockArray[5];

typedef struct {
	uint32_t position[2];
	int color;
	int lifetime;
	int index;
} Cube;

uint16_t Cube_Colors[] = {LCD_RED, LCD_BLUE, LCD_GREEN, LCD_YELLOW, LCD_MAGENTA, LCD_CYAN, LCD_WHITE, LCD_GREY};

unsigned long NumCube;  		// Number of Cube created
unsigned long NumSamples;   		// Incremented every ADC sample, in Producer
unsigned long UpdateWork;   		// Incremented every update on position values
unsigned long Calculation;  		// Incremented every cube number calculation

//---------------------User debugging-----------------------
unsigned long DataLost;     // data sent by Producer, but not received by Consumer
long MaxJitter;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
unsigned long const JitterSize=JITTERSIZE;
unsigned long JitterHistogram[JITTERSIZE]={0,};
unsigned long TotalWithI1;
unsigned short MaxWithI1;


void Device_Init(void){
	UART_Init();
	BSP_LCD_OutputInit();
	BSP_Joystick_Init();
	BSP_LED_init();
}
//------------------Task 1--------------------------------
// background thread executed at 20 Hz
//******** Producer *************** 
int UpdatePosition(uint16_t rawx, uint16_t rawy, jsDataType* data){
	if (rawx > origin[0]){
		x = x + ((rawx - origin[0]) >> 9);
	}
	else{
		x = x - ((origin[0] - rawx) >> 9);
	}
	if (rawy < origin[1]){
		y = y + ((origin[1] - rawy) >> 9);
	}
	else{
		y = y - ((rawy - origin[1]) >> 9);
	}
	if (x > 127){
		x = 127;}
	if (x < 0){
		x = 0;}
	if (y > 112 - CROSSSIZE){
		y = 112 - CROSSSIZE;}
	if (y < 0){
		y = 0;}
	data->x = x; data->y = y;
	return 1;
}

void Producer(void){
	uint16_t rawX,rawY; // raw adc value
	uint8_t select;
	jsDataType data;
	unsigned static long LastTime;  // time at previous ADC sample
	unsigned long thisTime;         // time at current ADC sample
	long jitter;                    // time between measured and expected, in us
	if (NumSamples < RUNLENGTH){
		BSP_Joystick_Input(&rawX,&rawY,&select);
		thisTime = OS_Time();       // current time, 12.5 ns
		UpdateWork += UpdatePosition(rawX,rawY,&data); // calculation work
		NumSamples++;               // number of samples
		if(JsFifo_Put(data) == 0){ // send to consumer
			DataLost++;
		}
	//calculate jitter
		if(UpdateWork > 1){    // ignore timing of first interrupt
			unsigned long diff = OS_TimeDifference(LastTime,thisTime);
			if(diff > PERIOD){
				jitter = (diff-PERIOD+4)/8;  // in 0.1 usec
			}
			else{
				jitter = (PERIOD-diff+4)/8;  // in 0.1 usec
			}
			if(jitter > MaxJitter){
				MaxJitter = jitter; // in usec
			}       // jitter should be 0
			if(jitter >= JitterSize){
				jitter = JITTERSIZE-1;
			}
			JitterHistogram[jitter]++; 
		}
		LastTime = thisTime;
	}
}

//--------------end of Task 1-----------------------------

//------------------Task 2--------------------------------
// background thread executes with SW1 button
// one foreground task created with button push
// foreground treads run for 2 sec and die
// ***********ButtonWork*************

void Laser(void){
		//	int i;
	while(NumSamples < RUNLENGTH){
		bulletv = 1;
		// for (i=0;i<5;i++){
		// 	OS_bWait(&LCDFree);
		// 	BSP_LCD_DrawBullet(bulletx, bullety, LCD_BLACK);
		// 	BSP_LCD_DrawBullet(prevx, prevy, LCD_RED);
		// 	OS_bSignal(&LCDFree);
		// 	bulletx = prevx;
		// 	bullety = prevy;
		// 	OS_Sleep(4);
		// }



		OS_bWait(&LCDFree);
		BSP_LCD_DrawBullet(prevx, prevy, LCD_RED);
		OS_bSignal(&LCDFree);
		bulletx = prevx;
		bullety = prevy;


		OS_Sleep(15);

		OS_bWait(&LCDFree);
		BSP_LCD_DrawBullet(bulletx, bullety, LCD_BLACK);
		OS_bSignal(&LCDFree);
		bulletv = 0;
		OS_Kill();
	}
	OS_Kill();
 }



void GameBegin(void)	{
	int i;
	uint32_t StartTime,CurrentTime,ElapsedTime;
	for (i=0; i<5; ++i){
	BlockArray[i].available = 1;}
	StartTime = OS_MsTime();
	ElapsedTime = 0;
	OS_bWait(&LCDFree);
	BSP_LCD_FillScreen(BGCOLOR);
	while (NumSamples < RUNLENGTH && ElapsedTime < LIFETIME){
		CurrentTime = OS_MsTime();
		ElapsedTime = CurrentTime - StartTime;
		BSP_LCD_DrawStart();
		BSP_LCD_DrawString(5, 5, "Game begins!", LCD_WHITE);
	}
	BSP_LCD_FillScreen(BGCOLOR);
	OS_bSignal(&LCDFree);
  	OS_Kill();  // done, OS does not return from a Kill
	}


// void GameOver(void){
// 	while(!life){
// 	OS_bWait(&LCDFree);
// 		BSP_LCD_FillScreen(BGCOLOR);
// 		BSP_LCD_DrawString(5, 5, "Game Over!", LCD_WHITE);
// 		OS_Sleep(50);
// 		OS_bSignal(&LCDFree);
// 	}
// 	OS_bWait(&LCDFree);
// 	BSP_LCD_FillScreen(BGCOLOR);
// 	OS_bSignal(&LCDFree);
// 	OS_Kill();
// }

//************SW1Push*************
// Called when SW1 Button pushed
// Adds another foreground task
// background threads execute once and return
void SW1Push(void){
  if(OS_MsTime() > 20 ){ // debounce
    if(OS_AddThread(&Laser,128,2)){
			OS_ClearMsTime(); 
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}

//--------------end of Task 2-----------------------------

//------------------Task 3--------------------------------

//******** Consumer *************** 
// foreground thread, accepts data from producer
// Display crosshair and its positions
// inputs:  none
// outputs: none
void Consumer(void){
	while(NumSamples < RUNLENGTH){
		jsDataType data;
		JsFifo_Get(&data);
		OS_bWait(&LCDFree);
		BSP_LCD_DrawCrosshair_Black(prevx, prevy, LCD_BLACK); // Draw a black crosshair
		BSP_LCD_DrawCrosshair(data.x, data.y, LCD_RED); // Draw a red crosshair
		OS_bSignal(&LCDFree);
		prevx = data.x; 
		prevy = data.y;
	  OS_Suspend();
		
	}
  OS_Kill();  // done
}


// Random Number Generator (referenced in project document -> https://www.maximintegrated.com/cn/app-notes/index.mvp/id/4400)

typedef unsigned int uint;
uint lfsr31, lfsr32;

int shift_lfsr(uint *lfsr, uint polynomial_mask) {
    int feedback;

    feedback = *lfsr &1;
    *lfsr >>= 1;
    if(feedback == 1)
        *lfsr ^= polynomial_mask;
    return *lfsr;
}

void init_lfsrs(void) {
    lfsr32 = 0xABCDEFAB; // 1st Seed Value
    lfsr31 = NVIC_ST_CURRENT_R; // 2nd Seed Value
}

int Random(void) { // returns random number from 0-1
    shift_lfsr(&lfsr32, POLY_MASK_32);
    return ((shift_lfsr(&lfsr32, POLY_MASK_32) ^ shift_lfsr(&lfsr31, POLY_MASK_31)) & 0xFFFF) % 2;
}

int RandomCube(void) { // return random number from 0 - 11
    shift_lfsr(&lfsr32, POLY_MASK_32);
    return ((shift_lfsr(&lfsr32, POLY_MASK_32) ^ shift_lfsr(&lfsr31, POLY_MASK_31)) & 0xFFFF) % 11;
}

int RandomDirection(void) { // returns random number from 0 - 3
    shift_lfsr(&lfsr32, POLY_MASK_32);
    return ((shift_lfsr(&lfsr32, POLY_MASK_32) ^ shift_lfsr(&lfsr31, POLY_MASK_31)) & 0xFFFF) % 4;
}

int RandomDirectionGame(void) { // returns random number from 0 - 2
    shift_lfsr(&lfsr32, POLY_MASK_32);
    return ((shift_lfsr(&lfsr32, POLY_MASK_32) ^ shift_lfsr(&lfsr31, POLY_MASK_31)) & 0xFFFF) % 3;
}

int RandomColor(void) {  // returns random number from 0 - 7
    shift_lfsr(&lfsr32, POLY_MASK_32);
    return ((shift_lfsr(&lfsr32, POLY_MASK_32) ^ shift_lfsr(&lfsr31, POLY_MASK_31)) & 0xFFFF) % 8;
}


void CubeNumCalc(void){ 
	uint16_t CurrentX,CurrentY;
    while(NumSamples < RUNLENGTH){
			CurrentX = x; CurrentY = y;
			area[0] = CurrentX / 22;
			area[1] = CurrentY / 20;
			Calculation++;
		}
		OS_Kill();
  }
//--------------end of Task 4-----------------------------


//************ Display *************** 
// foreground thread, do some pseudo works to test if you can add multiple periodic threads
// inputs:  none
// outputs: none
void Display(void){
	while(NumSamples < RUNLENGTH){
		OS_bWait(&LCDFree);
		BSP_LCD_Message(1,5,0,"Score:", score);		
		BSP_LCD_Message(1,5,11,"Life:", life);
		OS_bSignal(&LCDFree);
		OS_Suspend();
	}
  OS_Kill();  // done
}

//--------------end of Task 6-----------------------------


//------------------Task Cube--------------------------------
// inputs:  cubed: direction of the cube(0: up, 1: left, 2 : right, 3: down)
// outputs: none
void UpdateCube(Cube *cube){
	  int cubex;
	  int cubey;
	  int cubed;
	  int new_cubed;
//		int loc;
		int i;
		int change;
        BSP_LCD_DrawCube_Black(cube->position[0], cube->position[1], LCD_BLACK); // Draw a black cube

	    cubex = cube->position[0];
	    cubey = cube->position[1];
	    cubed = RandomDirectionGame()+1;

				do{
						if (cubed==0){
							cubey--;
						}else if(cubed==1){
							cubex--;
						}else if(cubed==2){
							cubex++;
						}else if(cubed==3){
							cubey++;
						}
					if(cubex<0||cubex>10){
						new_cubed=RandomDirectionGame()+1;
						while (new_cubed==cubed){
							new_cubed=RandomDirectionGame()+1;
						}
						cubed = new_cubed;
					}

						if(cubex<0){
							cubex=0;
						}else if(cubex>10){
							cubex=10;
						}
						if(cubey<0){
							cubey=0;
						}else if(cubey>11){
							cubey=11;
						}
					change = 0;
					for (i=0; i<5; ++i){
						if ((!BlockArray[i].available) && BlockArray[i].position[0]==cubex && BlockArray[i].position[1]==cubey){
							change = 1;
						}
					}
				}		
				while(change);

				BSP_LCD_DrawCube(cubex, cubey, Cube_Colors[cube->color]);
				cube->position[0]=cubex;
				cube->position[1]=cubey;
				// location[cube->index] = cubex*10+cubey;	
				BlockArray[cube->index].position[0] = cubex;
				BlockArray[cube->index].position[1] = cubey;
}

void CubeThread (void){
		int i;
		int index;
//		int loc;
		int update;
    Cube cube;  //Cube Initialization
    cube.position[0] = RandomCube();
    cube.position[1]= 0;
	update = 0;

	for (i=0; i<5; i++){
		if ((!BlockArray[i].available) && BlockArray[i].position[0]==cube.position[0] && BlockArray[i].position[1]==cube.position[1])
		{	cube.position[0] = RandomCube();
			cube.position[1]= 0;
			i = 0;
		}
	}

	index = 0;
	while(!BlockArray[index].available){
		index++;
	}
	// location[index] = loc;
	BlockArray[index].position[0] = cube.position[0];
	BlockArray[index].position[1] = cube.position[1];
	BlockArray[index].available = 0;
	cube.index = index;
    cube.color=RandomColor();
    cube.lifetime=15;

    OS_bWait(&LCDFree);
    BSP_LCD_DrawCube(cube.position[0], cube.position[1], Cube_Colors[cube.color]);
	OS_bSignal(&LCDFree);

			

			while(life && NumSamples < RUNLENGTH){
				update++;
				
				if(bulletv){
					 if((cube.position[0]*11) < bulletx && (cube.position[0]*11+11) > bulletx && (cube.position[1]*10) <= bullety){
							OS_bWait(&LCDFree);
							BSP_LCD_DrawCube_Black(cube.position[0], cube.position[1], LCD_BLACK);
							OS_bSignal(&LCDFree);
							score++;
							NumCube--;
							// location[index] = 99;
							BlockArray[index].available = 1;
							OS_Kill();
						}
				}
				else if ((x > cube.position[0]*11-5) && (x < cube.position[0]*11 + 16) && (y > cube.position[1]*10-5) && (y < cube.position[1]*10 + 15)){
		        	OS_bWait(&LCDFree);
		        	BSP_LCD_DrawCube_Black(cube.position[0], cube.position[1], LCD_BLACK);
					OS_bSignal(&LCDFree);
					// score++;
					life--;
					NumCube--;
					// location[index] = 99;
					BlockArray[index].available = 1;
					OS_Kill();
					}



					else{
			    	if (update==100){
						OS_bWait(&LCDFree);
						UpdateCube(&cube);
						OS_bSignal(&LCDFree);
						update=0;
						if(cube.position[1]==11){
							life--;
							NumCube--;
							// location[index] = 99;
							BlockArray[index].available = 1;
							
					        OS_bWait(&LCDFree);
					        BSP_LCD_DrawCube_Black(cube.position[0], cube.position[1], LCD_BLACK);
							OS_bSignal(&LCDFree);
							OS_Kill();
						}
					}
				}
				OS_Sleep(4);
			}
			OS_bWait(&LCDFree);
			BSP_LCD_DrawCube_Black(cube.position[0], cube.position[1], LCD_BLACK);
			OS_bSignal(&LCDFree);
			NumCube--;
			// location[index] = 99;
			BlockArray[index].available = 1;
			OS_Kill();
}

//Monitor the number of Cubes, increase the number of Cubes
void CubeRefresh(void){
	int i;
	while(NumSamples < RUNLENGTH){
		if(NumCube==0&&life>0){
			OS_Sleep(200);
		    for (i=0; i<RandomDirection()+1; ++i){
		    NumCube += OS_AddThread(&CubeThread, 128, 2);
		    }
			OS_Suspend();
		}else if(!life){
			NumSamples = RUNLENGTH;
			OS_Sleep(50);
			OS_bWait(&LCDFree);
			BSP_LCD_FillScreen(BGCOLOR);
			BSP_LCD_DrawString(5, 5, "Game Over!", LCD_WHITE);
			OS_bSignal(&LCDFree);
			OS_Kill();
		}else{
		OS_Suspend();
		}
	}
	OS_Kill();
}

//------------------Task 7--------------------------------
// background thread executes with button2
// one foreground task created with button push
// ***********ButtonWork2*************
void Restart(void){
	int i;
	uint32_t StartTime,CurrentTime,ElapsedTime;
	restartCount++;
	NumSamples = RUNLENGTH; // first kill the foreground threads
	OS_Sleep(50); // wait
	StartTime = OS_MsTime();
	ElapsedTime = 0;
	OS_bWait(&LCDFree);
	BSP_LCD_FillScreen(BGCOLOR);
	while (ElapsedTime < 500){
		CurrentTime = OS_MsTime();
		ElapsedTime = CurrentTime - StartTime;
		BSP_LCD_DrawString(5,6,"Restarting",LCD_WHITE);
	}
	BSP_LCD_FillScreen(BGCOLOR);
	OS_bSignal(&LCDFree);
	// restart
	for (i=0; i<5; ++i){
		// location[i] = 99;
		BlockArray[i].available = 1;
	}
	DataLost = 0;        // lost data between producer and consumer
  	NumSamples = 0;
  	UpdateWork = 0;
	MaxJitter = 0;       // in 1us units
	PseudoCount = 0;
	life = 5;
    score = 0;
	x = 63; y = 63;
	OS_AddThread(&Consumer,128,1); 
	OS_AddThread(&Display,128,3);
	OS_AddThread(&CubeRefresh,128, 2);
  for (i=0; i<RandomDirection()+1; ++i){
  NumCube += OS_AddThread(&CubeThread, 128, 2);
  }
	OS_Kill();  // done, OS does not return from a Kill
} 

//************SW2Push*************
// Called when Button2 pushed
// Adds another foreground task
// background threads execute once and return
void SW2Push(void){
  if(OS_MsTime() > 20 ){ // debounce
    OS_AddThread(&Restart,128,4);
    OS_ClearMsTime();  // at least 20ms between touches
  }
}

//--------------end of Task 7-----------------------------

// Fill the screen with the background color
// Grab initial joystick position to bu used as a reference
void CrossHair_Init(void){
	BSP_LCD_FillScreen(BGCOLOR);
	BSP_Joystick_Input(&origin[0],&origin[1],&select);
}

void LED_worker(void) {
	if (LedOn) {
		if (life > 3)
			BSP_LED_setGreen();
		else if (life > 1)
			BSP_LED_setYellow();
		else {
			BSP_LED_off();
			LedOn = 0;
		}
	} else {
		if (life > 3)
			BSP_LED_setGreen();
		else if (life > 1)
			BSP_LED_setYellow();
		else
			BSP_LED_setRed();
		LedOn = 1;
	}
}

//******************* Main Function**********
int main(void){ 
  int i;
  OS_Init();           // initialize, disable interrupts
	Device_Init();
  CrossHair_Init();
  init_lfsrs();
  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;
  MaxJitter = 0;       // in 1us units
//********initialize communication channels
  JsFifo_Init();

//*******attach background tasks***********
  OS_AddSW1Task(&SW1Push, 4);
	OS_AddSW2Task(&SW2Push, 4);
  OS_AddPeriodicThread(&Producer, PERIOD, 3); // 2 kHz real time sampling of PD3
	OS_AddPeriodicThread(&LED_worker, LED_BLINK_PERIOD, 4);
  NumCube = 0;
// create initial foreground threads
	OS_AddThread(&GameBegin, 128, 4);
  	OS_AddThread(&Consumer, 128, 1); 	
	OS_AddThread(&Display, 128, 3);	
	OS_AddThread(&CubeRefresh,128, 2);
  for (i=0; i<RandomDirection()+1; ++i){
  NumCube += OS_AddThread(&CubeThread, 128, 2);
  }
	
	//BSP_LED_setGreen();
	LedOn = 1;
 
  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
	return 0;            // this never executes
}
