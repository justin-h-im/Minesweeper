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
#define NUM_TASKS 2

// period definitions
const unsigned long LCD_PERIOD = 10;
const unsigned long JOYSTICK_PERIOD = 20;
const unsigned long GCD_PERIOD = 10;

// task array
task tasks[NUM_TASKS];

// task enums
enum LCD_States { LCD_Init, LCD_Display };
enum Joystick_States { Joystick_Run };

// task definitions
int LCD_Tick(int state) {
  switch (state) {
    case LCD_Init:
      // initialize the LCD
      lcdInit();
      state = LCD_Display;
      break;
    // within here, update depending on inputs from joystick, buttons, etc
    case LCD_Display:

      // check selection
      grid[gridX][gridY].selected = true;

      // display something on the LCD
      drawScreen();
      state = LCD_Display; // Stay in this state
      break;
    default:
      break;
  }
  return state;
}

int Joystick_Tick(int state) {
  static uint8_t prevGridX = 0;
  static uint8_t prevGridY = 0;
  static uint8_t prevSelect = 0;
  uint8_t press = GetBit(PORTC,PC2);

  switch (state) {
    case Joystick_Run:
      // read inputs
      uint16_t x = ADC_read(JOYSTICK_VRX);
      uint16_t y = ADC_read(JOYSTICK_VRY);
      // serial_println(map_value(0, 1023, 0, 2, x));
      // serial_println(map_value(0, 1023, 0, 2, y));
      
      if (map_value(0, 1023, 0, 2, x) == 2 && (gridX <= 7)) {
        gridX++;
      } else if (map_value(0, 1023, 0, 2, x) == 0 && (gridX > 0)) {
        gridX--;
      }
      
      if (map_value(0, 1023, 0, 2, y) == 2 && (gridY <= 7)) {
        gridY++;
      } else if (map_value(0, 1023, 0, 2, y) == 0 && (gridY > 0)) {
        gridY--;
      }



      serial_println(gridX);
      serial_println(gridY);
      // output
      grid[prevGridX][prevGridY].selected = false;
      grid[gridX][gridY].selected = true;

      state = Joystick_Run; 
      break;
    default:
      break;
  }
  prevGridX = gridX;
  prevGridY = gridY;
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

  // ADC initialization
  ADC_init();

  // LCD initialization
  lcdInit();

  // serial initialization
  serial_init(9600);

  // concurrent fsm task initialization
  tasks[0].state = LCD_Init; // Task initial state
  tasks[0].period = LCD_PERIOD; // Task period
  tasks[0].elapsedTime = tasks[0].period; // Task elapsed time
  tasks[0].TickFct = &LCD_Tick; // Task tick function

  tasks[1].state = Joystick_Run; // Task initial state
  tasks[1].period = JOYSTICK_PERIOD; // Task period
  tasks[1].elapsedTime = tasks[1].period; // Task elapsed time
  tasks[1].TickFct = &Joystick_Tick; // Task tick function

  // timer initialization
  TimerSet(GCD_PERIOD);
  TimerOn();

  // main loop (should never reach here)
  while (1) { }  // fill screen with red

  return 0;
}

