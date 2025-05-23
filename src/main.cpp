// all of these headers are provided from the CS120B course
#include "helper.h"
#include "spiAVR.h"
#include "periph.h"
#include "serialATmega.h"
#include "timerISR.h"
#define F_CPU 16000000UL // 16 MHz
#include <util/delay.h>
#include "main.h"

// Task struct for concurrent synchSMs implmentations (provided)
typedef struct _task{
  //Task's current state
	signed char state;
  // Task period
	unsigned long period;
  //Time elapsed since last task tick
	unsigned long elapsedTime;
  //Task tick function
	int (*TickFct)(int); 		
} task;

// task array size define
#define NUM_TASKS 1

// period definitions
const unsigned long LCD_PERIOD = 50;
const unsigned long GCD_PERIOD = 50;

// task array
task tasks[NUM_TASKS];

// task enums
enum LCD_States { LCD_Init, LCD_Display };

// task definitions
int LCD_Tick(int state) {
  switch (state) {
    case LCD_Init:
      // Initialize the LCD
      lcdInit();
      state = LCD_Display;
      break;
    // within here, update depending on inputs from joystick, buttons, etc
    case LCD_Display:
      // Display something on the LCD
      drawScreen();
      state = LCD_Display; // Stay in this state
      break;
    default:
      break;
  }
  return state;
}

// executes tasks
void TimerISR() {   
  // Iterate through each task in the task array
	for (unsigned int i = 0; i < NUM_TASKS; i++ ) {
    // Check if the task is ready to tick    
		if ( tasks[i].elapsedTime == tasks[i].period ) {
      // Tick and set the next state for this task      
			tasks[i].state = tasks[i].TickFct(tasks[i].state); 
      // Reset the elapsed time for the next tick
			tasks[i].elapsedTime = 0;                          
		}
    // Increment the elapsed time by GCD_PERIOD
		tasks[i].elapsedTime += GCD_PERIOD;
	}
}

int main() {
  // gpio initialization
  gpioInit();

  // LCD initialization
  lcdInit();

  // concurrent fsm task initialization
  tasks[0].state = LCD_Init; // Task initial state
  tasks[0].period = GCD_PERIOD; // Task period
  tasks[0].elapsedTime = tasks[0].period; // Task elapsed time
  tasks[0].TickFct = &LCD_Tick; // Task tick function

  // timer initialization
  TimerSet(GCD_PERIOD);
  TimerOn();

  // main loop (should never reach here)
  while (1) { }  // fill screen with red

  return 0;
}

