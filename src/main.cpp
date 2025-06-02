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
#define NUM_TASKS 3

// period definitions
const unsigned long LCD_PERIOD = 20;
const unsigned long JOYSTICK_PERIOD = 30;
const unsigned long GAME_PERIOD = 50;
const unsigned long GCD_PERIOD = 10;

// task array
task tasks[NUM_TASKS];

// task enums
enum LCD_States { LCD_Init, LCD_Display };
enum Joystick_States { Joystick_Run };
enum Game_States { Game_Run, Game_Lose, Game_Won };

// task definitions
int LCD_Tick(int state) {
  switch (state) {
    case LCD_Init:
      // initialize the LCD
      lcdInit();
      initGrid();
      state = LCD_Display;
      break;
    // within here, update depending on inputs from joystick, buttons, etc
    case LCD_Display:
      if (gameLost) { fillRect(4, 4, 131, 131, RED); }
      else {
        // display something on the LCD
        drawScreen();
      }


      state = LCD_Display;
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
  static uint16_t pressDurationCounter = 0;
  static bool longPressDetected = false;
  static uint8_t debounceCounter = 0;
  static uint16_t x_filter = 512;
  static uint16_t y_filter = 512;
  uint8_t press = !GetBit(PINC,2);

  switch (state) {
    case Joystick_Run:
      uint16_t x_raw = ADC_read(JOYSTICK_VRX);
      uint16_t y_raw = ADC_read(JOYSTICK_VRY);
      
      x_filter = (x_filter * 3 + x_raw) / 4;
      y_filter = (y_filter * 3 + y_raw) / 4;
      

      // 0=left/up, 1=dead, 2=center, 3=dead, 4=right/down
      uint8_t x_zone = 2;
      uint8_t y_zone = 2; 
      
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
      
      if (debounceCounter > 0) {
        debounceCounter--;
      } else {
        if (x_zone == 4 && gridX < 7) {
          gridX++;
          debounceCounter = 3; 
        } else if (x_zone == 0 && gridX > 0) {
          gridX--;
          debounceCounter = 3;
        }
        
        if (y_zone == 4 && gridY < 7) {
          gridY++;
          debounceCounter = 3;
        } else if (y_zone == 0 && gridY > 0) {
          gridY--;
          debounceCounter = 3;
        }
      }


      grid[prevGridX][prevGridY].selected = false;
      grid[gridX][gridY].selected = true;
    

      if (press) {
        if (pressDurationCounter < 0xFFFF) {
            pressDurationCounter++;
        }

        if (pressDurationCounter >= 10 && !longPressDetected) {
            if (!grid[gridX][gridY].revealed) {
                grid[gridX][gridY].flagged = !grid[gridX][gridY].flagged;
                serial_println("Long Press: Toggled Flag at (");
                serial_println(gridX);
                serial_println(", ");
                serial_println(gridY);
                serial_println(")");
            }
            longPressDetected = true;
        }
      } 
      else {
        if (prevPress && !longPressDetected) {
            if (!grid[gridX][gridY].revealed && !grid[gridX][gridY].flagged) {
              grid[gridX][gridY].revealed = true;
            }
        }
        pressDurationCounter = 0;
        longPressDetected = false;
      
        serial_println(gridX);
        serial_println(gridY);
        serial_println(press && !prevPress);

      }

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

int Game_Tick(int state) {
  static int numMines = 0;


  switch (state) {
    case Game_Run:
      if (gameLost) { state = Game_Lose; }
      break;

    case Game_Won:
      break;

    case Game_Lose:
      fillRect(0, 0, 127, 127, RED);
  }
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

  tasks[2].state = Game_Run; // Task initial state
  tasks[2].period = GAME_PERIOD; // Task period
  tasks[2].elapsedTime = tasks[2].period; // Task elapsed time
  tasks[2].TickFct = &Game_Tick; // Task tick function

  // timer initialization
  TimerSet(GCD_PERIOD);
  TimerOn();

  // main loop (should never reach here)
  while (1) { }
  
  return 0;
}

