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
const unsigned long LCD_PERIOD = 20;
const unsigned long JOYSTICK_PERIOD = 30;
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
  static uint8_t prevPress = 0;
  static uint8_t debounceCounter = 0;
  static uint16_t x_filter = 512;  // Running average for X
  static uint16_t y_filter = 512;  // Running average for Y
  uint8_t press = !GetBit(PINC,2);

  switch (state) {
    case Joystick_Run:
      // Read raw ADC values
      uint16_t x_raw = ADC_read(JOYSTICK_VRX);
      uint16_t y_raw = ADC_read(JOYSTICK_VRY);
      
      // Simple low-pass filter to reduce noise
      x_filter = (x_filter * 3 + x_raw) / 4;
      y_filter = (y_filter * 3 + y_raw) / 4;
      
      // Use wider dead zones for more stable operation
      // Create 5 zones: 0=left/up, 1=dead, 2=center, 3=dead, 4=right/down
      uint8_t x_zone = 2; // Default to center
      uint8_t y_zone = 2; // Default to center
      
      if (x_filter < 300) {
        x_zone = 0;  // Left
      } else if (x_filter > 723) {
        x_zone = 4;  // Right
      }
      
      if (y_filter < 300) {
        y_zone = 0;  // Up
      } else if (y_filter > 723) {
        y_zone = 4;  // Down
      }
      
      // Debug output
      serial_println(x_zone);
      serial_println(y_zone);
      
      // Process movement with debouncing
      if (debounceCounter > 0) {
        debounceCounter--;
      } else {
        // X movement with proper bounds
        if (x_zone == 4 && gridX < 7) {  // Right
          gridX++;
          debounceCounter = 3; // Longer debounce for smoother movement
        } else if (x_zone == 0 && gridX > 0) {  // Left
          gridX--;
          debounceCounter = 3;
        }
        
        // Y movement with proper bounds
        if (y_zone == 4 && gridY < 7) {  // Down
          gridY++;
          debounceCounter = 3;
        } else if (y_zone == 0 && gridY > 0) {  // Up
          gridY--;
          debounceCounter = 3;
        }
      }


      // Clear previous selection and set new one
      grid[prevGridX][prevGridY].selected = false;
      grid[gridX][gridY].selected = true;
      
      // Check if the joystick is pressed
      if (press && !prevPress) {
        // if pressed, toggle the revealed state
        if (!grid[gridX][gridY].revealed) {
          grid[gridX][gridY].revealed = !grid[gridX][gridY].revealed;
          grid[gridX][gridY].selected = false;
        }

      }
      
      serial_println(gridX);
      serial_println(gridY);
      serial_println(press && !prevPress);



      state = Joystick_Run; 
      break;
    default:
      break;
  }
  prevPress = press;
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
  while (1) { }
  
  return 0;
}

