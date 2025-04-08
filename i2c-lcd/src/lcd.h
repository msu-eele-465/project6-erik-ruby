#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stdio.h>
#include <msp430fr2310.h>

// delays
#define DELAY_0001  __delay_cycles(1000)         // 0.001 s
#define DELAY_001   __delay_cycles(10000)        // 0.01 s

// LCD Commands
#define LCD_FUNCTION            0x28    // 4 bit, 2 lines

#define LCD_CLEAR_DISPLAY       0x01
#define LCD_RETURN_HOME         0x02
#define LCD_BOTTOM_LINE         0xC0    // set DDRAM to address 40h
#define LCD_ENTRY_MODE_SET      0x06    // Cursor right, DDRAM +1
#define LCD_DISPLAY_ON          0x0C    // Display ON/Cursor OFF
#define LCD_CURSOR_ON           0x0E    // Display ON/Cursor ON
#define LCD_CURSOR_BLINK        0x0F    // Blink ON
#define LCD_DISPLAY_OFF         0x08    // Display OFF
#define LCD_CURSOR_RIGHT        0x14    // Shift cursor right

#define lcd_send_command(cmd) lcd_send((cmd), 0)
#define lcd_send_data(data)   lcd_send((data), 1)

#define WINDOW_CHANGE           0x41      // A
#define PATTERN_CHANGE          0x42      // B
#define TEMP_UNIT_CHANGE        0x43      // C
#define LOCK                    0x44      // D
#define CHANGE_PERMITTED        0x47      // G

/**
* initialize lcd outputs and begin startup process
*/
void init_lcd();


/**
* send character codes for data
* 
* @param: byte of data
*/
void lcd_send(uint8_t data, uint8_t is_data);

/**
* send string of characters
* 
* @param: string to be sent
*/
void lcd_send_string(char *str);

/**
* select a string to send given a choice
* 
* @param: byte of data
*/
void lcd_choose_string(uint8_t data);

/**
* display the current pattern on the LCD
*/
void lcd_disp_pattern();

/**
* set window size
* 
* @param: byte of data (window size n)
*/
void lcd_set_window_size(uint8_t data);

/**
* set temperature to the 3 digits that've been sent over
*/
void lcd_set_temperature();

/**
* toggle cursor on lcd
*/
void lcd_toggle_cursor();

/**
* toggle cursor blink on lcd
*/ 
void lcd_toggle_blink();

/**
* lcd initialization
*/ 
void lcd_set_function();

/**
* clear line
*/ 
void lcd_clear_line(uint8_t cmd);

#endif
