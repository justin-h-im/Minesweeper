#include "helper.h"
#include "spiAVR.h"
#include "periph.h"
#include "serialATmega.h"
#include "timerISR.h"
#include "graphics.h"
#include <avr/pgmspace.h>
#include <stdint.h>

// enums for cell status
typedef enum {
    EMPTY = 0,
    NUMBER_1 = 1,
    NUMBER_2 = 2,
    NUMBER_3 = 3,
    EXPLODED_MINE = 4,
    FLAG = 5
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
    bool selected = false;
    const uint16_t *gfx;
} cell;

// function defines
void gpioInit();
void lcdInit();
void initGrid();
void drawSquare(uint8_t x0, uint8_t y0, const uint16_t *gfx);
void drawScreen();
uint16_t readGraphicPixel(const uint16_t *gfx, uint8_t row, uint8_t col);
void fillRect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color);

// pin defines
// Port B
#define JOYSTICK_VRX PC0
#define JOYSTICK_VRY PC1
#define LCD_SDA PB3 // LCD Data Pin (MOSI) (output)
#define LCD_SCK PB5 // LCD Clock Pin (output)

// Port C
// #define LCD_LED PB4 // (output)
#define LCD_A0 PB0 // (output)
#define LCD_RESET PB1 // (output)
#define LCD_CS PB2 // LCD Chip Select (SS) (output)
#define I2C_SDA PC4
#define I2C_SCL PC5

// Port D
// #define START_BUTTON PD2
// #define RESET_BUTTON PD3
// #define PLAYER_BUTTON PD4
#define SELECT_BUTTON PC2
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



// global variables
cell grid[8][8]; // 8x8 grid of cells
uint8_t gridX = 0;
uint8_t gridY = 0;
bool gameLost = false;
bool gameWon = false;


// reads pixel from progmem
uint16_t readGraphicPixel(const uint16_t *gfx, uint8_t row, uint8_t col) {
    return pgm_read_word(&gfx[row * 16 + col]);
}

// send command to the LCD
void spiWriteCommand(uint8_t command) {
  // pull A0 low to specify command
  PORTB &= ~(1 << LCD_A0);
  // pull cs low
  PORTB &= ~(1 << LCD_CS);
  
  // send command, wait for transmission to complete
  SPDR = command;
  while (!(SPSR & (1 << SPIF))); 

  // pull cs high
  PORTB |= (1 << LCD_CS);
}

// send data to the LCD
void spiWriteData(uint8_t data) {

    // configure PC0 and PC1 as inputs
    PORTC |= ~(1 << JOYSTICK_VRX);
    PORTC |= ~(1 << JOYSTICK_VRY);

    // pull A0 high to specify data
    PORTB |= (1 << LCD_A0);
    // pull cs low
    PORTB &= ~(1 << LCD_CS);

    // send data, wait for transmission to complete
    SPDR = data;
    while (!(SPSR & (1 << SPIF))); 

    // pull cs high
    PORTB |= (1 << LCD_CS);
}

// GPIO initialization, call before tasks in main
void gpioInit() {
    // SPI pins
    // configure SCK and MOSI as output
    DDRB |= (1 << PB2);
    DDRB |= (1 << LCD_SCK) | (1 << LCD_SDA);
    // configure LED, A0, Reset, and CS as output
    DDRB |= (1 << LCD_A0) | (1 << LCD_RESET) | (1 << LCD_CS);

    // configure joystick pins as input
    DDRC &= ~(1 << JOYSTICK_VRX) & ~(1 << JOYSTICK_VRY) & ~(1 << SELECT_BUTTON);

    PORTB |= (1 << PB2); // pull SS
    PORTB |= (1 << LCD_CS); // pull cs high
    PORTB |= (1 << LCD_A0); // pull A0 high
    PORTB |= (1 << LCD_RESET); // pull reset high

    // enable spi, set as master
    SPCR = (1<<SPE) | (1<<MSTR);
    // SPSR = (1 << SPI2X);
}

// LCD initialization
void lcdInit() {
    // lcd initialization, refer to st7735 datasheet for details

    // hardware reset
    PORTB &= ~(1 << LCD_RESET); // bring reset low
    _delay_ms(200);
    PORTB |= (1 << LCD_RESET); // bring reset high
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

    // fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK); // fill screen with black

    // create grid
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            grid[i][j].status = EMPTY;
            grid[i][j].revealed = false;
            grid[i][j].flagged = false;
            grid[i][j].selected = false;
            grid[i][j].gfx = (const uint16_t*)emptyUnrevealedGrid;
        }
    }
}

// grid initialization
void initGrid() {
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            grid[i][j].status = EMPTY;
            grid[i][j].revealed = false;
            grid[i][j].flagged = false;
            grid[i][j].selected = false;
            grid[i][j].gfx = (const uint16_t*) emptyUnrevealedGrid;
        }
    }
    
    // random mine placement
    int minesPlaced = 0;
    while (minesPlaced < 7) {
        static uint16_t lfsr = 0xACE1; // randomly chosen seed
        lfsr = (lfsr >> 1) ^ (-(lfsr & 1) & 0xB400); 
        
        uint8_t x = lfsr % ROWS; 
        uint8_t y = (lfsr / ROWS) % COLS; 
        
        // check if position is already a mine
        if (grid[x][y].status != EXPLODED_MINE) {
            grid[x][y].status = EXPLODED_MINE;
            minesPlaced++;
            serial_println(x);
            serial_println(y);
            serial_println(grid[x][y].status);
        }
    }
    
    // calculate numbers for non-mine cells
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            if (grid[i][j].status != EXPLODED_MINE) {
                uint8_t mineCount = 0;
                
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0) continue; 
                        int ni = i + di;
                        int nj = j + dj;
                        
                        if (ni >= 0 && ni < ROWS && nj >= 0 && nj < COLS) {
                            if (grid[ni][nj].status == EXPLODED_MINE) {
                                mineCount++;
                            }
                        }
                    }
                }
                
                switch (mineCount) {
                    case 0:
                        grid[i][j].status = EMPTY;
                        break;
                    case 1:
                        grid[i][j].status = NUMBER_1;
                        break;
                    case 2:
                        grid[i][j].status = NUMBER_2;
                        break;
                    case 3:
                        grid[i][j].status = NUMBER_3;
                        break;
                    default:
                        grid[i][j].status = NUMBER_3;
                        break;
                }
            }
        }
    }
}

// draws an individual 16 x 16 square
// code: 0 = no mines, 1 = number 1, 2 = number 2, 3 = number 3, 4 = exploded mine, 5 = flag
void drawSquare(uint8_t x0, uint8_t y0, const uint16_t *gfx) {
  spiWriteCommand(CASET);
  spiWriteData(0); spiWriteData(x0);
  spiWriteData(0); spiWriteData(x0 + 15);
  spiWriteCommand(RASET);
  spiWriteData(0); spiWriteData(y0);
  spiWriteData(0); spiWriteData(y0 + 15);


  spiWriteCommand(RAMWR);
  for (uint8_t row = 0; row < 16; ++row) {
    for (uint8_t col = 0; col < 16; ++col) {
      uint16_t color = readGraphicPixel(gfx, row, col);
      spiWriteData(color >> 8);
      spiWriteData(color & 0xFF);
    }
  }
}

void drawScreen() {
    // draw the screen
    for (uint8_t j = 0; j < 8; ++j) {
        for (uint8_t i = 0; i < 8; ++i) {
            const uint16_t *g;

            // determine which graphic to show
            if (grid[i][j].revealed) {
                if (grid[i][j].selected) {
                    switch (grid[i][j].status) {
                    case EMPTY:
                        g = (const uint16_t*)emptyRevealedSelectedGrid;
                        break;
                    case NUMBER_1:
                        g = (const uint16_t*)number1SelectedGrid;
                        break;
                    case NUMBER_2:
                        g = (const uint16_t*)number2SelectedGrid;
                        break;
                    case NUMBER_3:
                        g = (const uint16_t*)number3SelectedGrid;
                        break;
                    case EXPLODED_MINE:
                        g = (const uint16_t*)explosionMine1pxGrid;
                        gameLost = true;
                        break;
                    case FLAG:
                        g = (const uint16_t*)flagSelectedGrid;
                        break;
                    default:
                        g = (const uint16_t*)emptyRevealedGrid;
                        break;
                    }
                    if (grid[i][j].flagged) { g = ( const uint16_t*) flagGrid; }
                }
                else {
                    // cell is revealed, show the actual content
                    switch (grid[i][j].status) {
                        case EMPTY:
                            g = (const uint16_t*)emptyRevealedGrid;
                            break;
                        case NUMBER_1:
                            g = (const uint16_t*)number1Grid;
                            break;
                        case NUMBER_2:
                            g = (const uint16_t*)number2Grid;
                            break;
                        case NUMBER_3:
                            g = (const uint16_t*)number3Grid;
                            break;
                        case EXPLODED_MINE:
                            g = (const uint16_t*)explosionMine1pxGrid;
                            gameLost = true;
                            break;
                        default:
                            g = (const uint16_t*)emptyRevealedGrid;
                            break;
                    }
                }

            } 

            else {
                if (grid[i][j].selected) { 
                    if (grid[i][j].flagged) { g = (const uint16_t*) flagSelectedGrid; }
                    else { g = (const uint16_t*) emptyUnrevealedSelectedGrid; }
                }
                else if (grid[i][j].flagged) { g = (const uint16_t*) flagGrid; }
                else { g = (const uint16_t*) emptyUnrevealedGrid; }
            }

            
            int x0 = (16 * i) + 4; // x0 coordinate of the square
            int y0 = (16 * j) + 4; // y0 coordinate of the square
            drawSquare(x0, y0, g);
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