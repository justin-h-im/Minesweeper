// all of these headers are provided from the CS120B course
#include "helper.h"
#include "spiAVR.h"
#include "periph.h"
#include "serialATmega.h"
#include "timerISR.h"
#define F_CPU 16000000UL // 16 MHz
#include <util/delay.h>

// helper functions declarations
// GPIO initialization, call before tasks in main
void gpioInit();
// LCD initialization, should be called after GPIO initialization
void lcdInit();

void spiWriteCommand(uint8_t command);

void spiWriteData(uint8_t data);

void fillRect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color);

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

// // pin defines
// /*
//   PB0 -> Joystick Vrx
//   PB1 -> Joystick Vry
//   PB3 -> LCD SDA Pin		
//   PB5-> LCD SCK Pin

//   PC0 -> LCD LED Pin
//   PC1 -> LCD A0 Pin
//   PC2 -> LCD Reset Pin
//   PC3 -> LCD CS Pin
//   PC4 -> I2C SDA
//   PC5 -> I2C SCL

//   PD2 -> Start Button
//   PD3 -> Reset Button
//   PD4 -> “1 or 2” Player Button
//   PD5 -> Select Button
//   PD6 -> Buzzer Output
// */

// Port B
#define JOYSTICK_VRX PB0
#define JOYSTICK_VRY PB1
#define LCD_SDA PB3 // LCD Data Pin (MOSI) (output)
#define LCD_SCK PB5 // LCD Clock Pin (output)

// Port C
#define LCD_LED PC0 // (output)
#define LCD_A0 PC1 // (output)
#define LCD_RESET PC2 // (output)
#define LCD_CS PB3 // LCD Chip Select (SS) (output)
#define I2C_SDA PC4
#define I2C_SCL PC5

// Port D
#define START_BUTTON PD2
#define RESET_BUTTON PD3
#define PLAYER_BUTTON PD4
#define SELECT_BUTTON PD5
#define BUZZER PD6

// LCD command defines
#define SWRESET 0x01
#define CASET 0x2A
#define RASET 0x2B
#define RAMWR 0x2C


// period definitions
const unsigned long GCD_PERIOD = 50;

// // task array
task tasks[NUM_TASKS];

// // task enums


// // task definitions


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
  fillRect(0, 0, 127, 127, 0xF800);
  // test, fill the screen with red

  // concurrent fsm task initialization

  // timer initialization
  // TimerSet(GCD_PERIOD);
  // TimerOn();


  // main loop (should never reach here)
  while (1) { fillRect(0, 0, 127, 127, 0x001F); }  // fill screen with red

  return 0;
}

// GPIO initialization
void gpioInit() {

  // SPI pins
  // configure SCK and MOSI as output
  DDRB |= (1 << LCD_SCK) | (1 << LCD_SDA) | (1 << PB2);;
  // configure LED, A0, Reset, and CS as output
  DDRC |= (1 << LCD_LED) | (1 << LCD_A0) | (1 << LCD_RESET) | (1 << LCD_CS);

  // GPIO initialization, refer to datasheet for details
  // Set pins as input or output
  // Set pull-up resistors if needed
  // Set initial states for outputs
  // etc.

  PORTC |= (1 << PB2); // pull SS
  PORTC |= (1 << LCD_CS); // pull cs high
  PORTC |= (1 << LCD_A0); // pull A0 high
  PORTC |= (1 << LCD_RESET); // pull reset high

  // enable spi, set as master
  SPCR = (1<<SPE) | (1<<MSTR);
  // SPSR = (1 << SPI2X);
}

// send command to the LCD
void spiWriteCommand(uint8_t command) {
  // pull A0 low to specify command
  PORTC &= ~(1 << LCD_A0);
  // pull cs low
  PORTC &= ~(1 << LCD_CS);
  
  // send command, wait for transmission to complete
  SPDR = command;
  while (!(SPSR & (1 << SPIF))); 

  // pull cs high
  PORTC |= (1 << LCD_CS);
}

// send data to the LCD
void spiWriteData(uint8_t data) {
  // pull A0 high to specify data
  PORTC |= (1 << LCD_A0);
  // pull cs low
  PORTC &= ~(1 << LCD_CS);
  
  // send data, wait for transmission to complete
  SPDR = data;
  while (!(SPSR & (1 << SPIF))); 

  // pull cs high
  PORTC |= (1 << LCD_CS);
}

// fills the enture screen wth a color
void fillRect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color) {
  uint32_t count = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);

  // Set window
  spiWriteCommand(CASET);
  spiWriteData(0x00); spiWriteData(x0);
  spiWriteData(0x00); spiWriteData(x1);

  spiWriteCommand(RASET);
  spiWriteData(0x00); spiWriteData(y0);
  spiWriteData(0x00); spiWriteData(y1);

  // Write pixels
  spiWriteCommand(RAMWR);
  for (uint32_t i = 0; i < count; ++i) {
    spiWriteData(color >> 8);
    spiWriteData(color & 0xFF);
  }
}

// LCD initialization
void lcdInit() {
  // lcd initialization, refer to st7735 datasheet for details

  // hardware reset
  PORTC &= ~(1 << LCD_RESET); // bring reset low
  _delay_ms(200);
  PORTC |= (1 << LCD_RESET); // bring reset high
  _delay_ms(200);

  // initialize SPI
  spiWriteCommand(0x01); // send SWRESET command
  _delay_ms(150);
  spiWriteCommand(0x11); // send SLPOUT command
  _delay_ms(200);

  spiWriteCommand(0x3A); // send COLMOD command
  spiWriteData(0x05); // set mode to 16-bit color
  _delay_ms(10);
  spiWriteCommand(0x29); // send DISPON command
  _delay_ms(200);
}