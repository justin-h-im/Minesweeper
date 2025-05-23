#include "helper.h"
#include "spiAVR.h"
#include "periph.h"
#include "serialATmega.h"
#include "timerISR.h"

// enums for cell status
typedef enum {
    EMPTY = 0,
    NUMBER_1,
    NUMBER_2,
    NUMBER_3,
    EXPLODED_MINE,
    FLAG
} CellStatus;

// structs
// 16 * 16 graphic cell
typedef struct _graphic {
    uint16_t graphicGrid[16][16];
} graphic;

typedef struct _cell {
    CellStatus status;
    bool revealed = false;
    bool flagged = false;
    graphic *gfx;
} cell;

// function defines
void gpioInit();
void lcdInit();
void drawSquare(uint8_t x0, uint8_t y0, graphic *gfx);
void drawScreen();
void fillRect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color);

// pin defines
// Port B
#define JOYSTICK_VRX PB0
#define JOYSTICK_VRY PB1
#define LCD_SDA PB3 // LCD Data Pin (MOSI) (output)
#define LCD_SCK PB5 // LCD Clock Pin (output)

// Port C
#define LCD_LED PC0 // (output)
#define LCD_A0 PC1 // (output)
#define LCD_RESET PC2 // (output)
#define LCD_CS PC3 // LCD Chip Select (SS) (output)
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

// dimension defines
#define LCD_WIDTH 127
#define LCD_HEIGHT 127

// rows and columns on the screen
#define ROWS 8
#define COLS 8

// define colors (BGR)
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define YELLOW 0xFFE0
#define BROWN 0x7B00
#define PURPLE 0xF81F

// global variables
cell grid[8][8]; // 8x8 grid of cells
graphic emptyUnrevealedGrid = {{
    { BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, BLACK },
    { BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK },
}};

graphic emptyRevealedGrid = {{
    { BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK },

}};

graphic number1Grid = {{
    { BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BLUE, BLUE, BLUE, BLUE, BLUE, BLUE, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK },
}};

graphic number2Grid = {{
    { BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, PURPLE, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, PURPLE, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, PURPLE, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, PURPLE, PURPLE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, PURPLE, PURPLE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, PURPLE, PURPLE, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, BROWN, PURPLE, BROWN, BROWN, BLACK },
    { BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK },
}};

graphic number3Grid = {{
    {},


}};


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

// GPIO initialization, call before tasks in main
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

    spiWriteCommand(0x36); // send MADCTL command
    spiWriteData(0xC8); // set memory data access control

    fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK); // fill screen with black

    // create grid
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            grid[i][j].status = EMPTY;
            grid[i][j].revealed = false;
            grid[i][j].flagged = false;
            grid[i][j].gfx = &emptyUnrevealedGrid;
        }
    }
}

// draws an individual 16 x 16 square
// code: 0 = no mines, 1 = number 1, 2 = number 2, 3 = number 3, 4 = exploded mine, 5 = flag
void drawSquare(uint8_t x0, uint8_t y0, graphic *gfx) {
  // 1) Set the 16×16 window on the display:
  spiWriteCommand(CASET);
  spiWriteData(0); spiWriteData(x0);
  spiWriteData(0); spiWriteData(x0 + 15);
  spiWriteCommand(RASET);
  spiWriteData(0); spiWriteData(y0);
  spiWriteData(0); spiWriteData(y0 + 15);

  // 2) Start RAM write:
  spiWriteCommand(RAMWR);

  // 3) Stream each pixel in row-major order:
  for (uint8_t row = 0; row < 16; ++row) {
    for (uint8_t col = 0; col < 16; ++col) {
      uint16_t color = gfx->graphicGrid[row][col];
      spiWriteData(color >> 8);
      spiWriteData(color & 0xFF);
    }
  }
}

void drawScreen() {
    // draw the screen
    // spiWriteCommand(RAMWR);
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            if (grid[i][j].revealed == false) {
                drawSquare((16 * i) + 4, (16 * j) + 4, &emptyUnrevealedGrid);
            }
            else if (grid[i][j].revealed == true) {
                drawSquare((16 * i) + 4, (16 * j) + 4, &emptyRevealedGrid);
            }
            // else {
            //     if (grid[i][j].status == EMPTY) {
            //         grid[i]
            //     }
            //     else if (grid[i][j].status == NUMBER_1) {
            //         grid[i][j].gfx = &number1Grid;
            //     }
            //     else if (grid[i][j].status == NUMBER_2) {
            //         grid[i][j].gfx = &number2Grid;
            //     }
            //     else if (grid[i][j].status == NUMBER_3) {
            //         grid[i][j].gfx = &number2Grid;
            //     }
            //     else if (grid[i][j].status == EXPLODED_MINE) {
            //         grid[i][j].gfx = &number2Grid;
            //     }
            //     else if (grid[i][j].status == FLAG) {
            //         grid[i][j].gfx = &number2Grid;
            //     }
            //     // draw the square
            //     drawSquare((16 * i) + 4, (16 * j) + 4, &number2Grid);
            // }

        }
    }
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

