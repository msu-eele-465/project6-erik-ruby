/**
* @file
* @brief LCD interface functionality
*
*/
#include "src/lcd.h"
#include "intrinsics.h"
#include "msp430fr2310.h"


uint8_t state_flag;         // 0 = just update temp, 1 = pattern, 2 = window
uint8_t current_n, current_temp_digit, current_pattern, temp_unit;
uint8_t change_allowed;   // flag to tell if the incoming data is allowed to change window size or n

char *lcd_strings[] = {
    "static", "toggle", "up counter", "in and out",
    "down counter", "rotate 1 left", "rotate 7 right", "fill left", "none"
};
char *window_state_str = "set window size";
char *pattern_state_str = "set pattern";
uint8_t temp_digits[3] = {'0','0','0'}; 

void init_lcd(){
    P1DIR |= BIT0 | BIT1 | BIT4 | BIT5 | BIT6 | BIT7;       // EN, RS, DB4, DB5, DB6, DB7
    P1OUT &= ~(BIT0 | BIT1 | BIT4 | BIT5 | BIT6 | BIT7);    // Clear outputs

    state_flag = 0;
    current_temp_digit = 0;
    current_n = '3';
    current_pattern = 8;                      // "none"
    temp_unit = 'C';
    change_allowed = 0;

    PM5CTL0 &= ~LOCKLPM5;
    
    __delay_cycles(50000);
    lcd_set_function();
    DELAY_001;
    lcd_send_command(LCD_DISPLAY_ON);
    DELAY_001;
    lcd_send_command(LCD_ENTRY_MODE_SET);
    DELAY_001;
    lcd_send_command(LCD_CURSOR_ON);
    DELAY_001;
    lcd_send_command(LCD_CURSOR_RIGHT);
    DELAY_001;
    lcd_send_command(LCD_CLEAR_DISPLAY);
}

void lcd_send(uint8_t data, uint8_t is_data){
    if (is_data)
        P1OUT |= BIT1;        // RS = 1 for data
    else
        P1OUT &= ~BIT1;       // RS = 0 for command

    P1OUT = (data & 0xF0);    // Send high nibble (on P1.4-7)
    P1OUT |= BIT0;            // Enable pulse
    DELAY_0001;
    P1OUT &= ~BIT0;

    DELAY_0001;
    P1OUT = ((data << 4) & 0xF0);
    if (is_data) P1OUT |= BIT1;   // Re-set RS again if needed (optional)
    P1OUT |= BIT0;
    DELAY_0001;
    P1OUT &= ~BIT0;
}

void lcd_send_string(char *str){
    while (*str != '\0'){
        lcd_send_data(*str++);
    }
}

void lcd_choose_string(uint8_t data) 
{   
    if (data == LOCK)               // always able to lock
    {
        DELAY_0001;
        lcd_send_command(LCD_CLEAR_DISPLAY);
    }
    else
    {
        if(change_allowed == 0)             // update temp unless it's a state change or temp disp change
        {
            switch(data)
            {
                case WINDOW_CHANGE:
                    lcd_clear_line(LCD_RETURN_HOME);
                    DELAY_0001;
                    lcd_send_string(window_state_str);
                    state_flag = 2;
                    break;
                case PATTERN_CHANGE:
                    lcd_clear_line(LCD_RETURN_HOME);
                    DELAY_0001;
                    lcd_send_string(pattern_state_str);
                    state_flag = 1;
                    break;
                case TEMP_UNIT_CHANGE:
                    if(temp_unit == 'C')
                    {
                        temp_unit = 'F';
                    }
                    else 
                    {
                        temp_unit = 'C';
                    }
                    lcd_set_temperature();
                    break;
                case CHANGE_PERMITTED:
                    // when the LCD recieves a 'G' it will automatically use the next
                    // byte of data to change pattern or window size
                    change_allowed = 1;
                    break;
                default:        // it's a temperature digit
                    temp_digits[current_temp_digit] = data;
                    current_temp_digit++;
                    if(current_temp_digit > 2)
                    {
                        current_temp_digit = 0;
                        lcd_set_temperature();
                    }   
                    break;
            }
        }
        else
        {
            if(state_flag == 1)        // update pattern
            {
                if (data >= '0' && data <= '7') 
                {
                    current_pattern = data - '0';
                    lcd_disp_pattern();
                } 
            }
            else                       // update window
            {
                lcd_set_window_size(data);
                lcd_disp_pattern();
            }
            state_flag = 0;
            change_allowed = 0;
        }
    }
}

void lcd_disp_pattern()
{
    lcd_clear_line(LCD_RETURN_HOME);
    DELAY_0001;
    lcd_send_string((char*)lcd_strings[current_pattern]);
}

void lcd_set_window_size(uint8_t data)
{
    current_n = data;
    // set DDRAM to right before bottom right corner
    lcd_send_command(0x80 | 0x4D);       
    DELAY_0001;
    lcd_send_data('N');            
    lcd_send_data('=');            
    lcd_send_data(data);        
}

void lcd_set_temperature()
{    
    lcd_clear_line(LCD_BOTTOM_LINE);
    DELAY_0001;
    lcd_send_data('T');            
    lcd_send_data('=');            
    lcd_send_data(temp_digits[0]);
    lcd_send_data(temp_digits[1]);
    lcd_send_data('.');
    lcd_send_data(temp_digits[2]);
    lcd_send_data(0b11011111);      // degree symbol         
    lcd_send_data(temp_unit);
    lcd_set_window_size(current_n);
    lcd_send_command(LCD_RETURN_HOME);           
}


void lcd_set_function(){
    P1OUT &= ~BIT1;                 // RS = 0 for command
    P1OUT = (0x03 << 4);            // Send high nibble
    P1OUT |= BIT0;                  // Enable pulse
    DELAY_0001;
    P1OUT &= ~BIT0;

    __delay_cycles(30000);

    P1OUT = (0x03 << 4);            // Send high nibble
    P1OUT |= BIT0;                  // Enable pulse
    DELAY_0001;
    P1OUT &= ~BIT0;

    __delay_cycles(30000);

    P1OUT = (0x03 << 4);          // Send high nibble
    P1OUT |= BIT0;                  // Enable pulse
    DELAY_0001;
    P1OUT &= ~BIT0;

    __delay_cycles(30000);

    P1OUT = (0x02 << 4);    // Send low nibble
    P1OUT |= BIT0;
    DELAY_0001;
    P1OUT &= ~BIT0;

    lcd_send_command(LCD_FUNCTION);
}

void lcd_clear_line(uint8_t cmd)
{
    lcd_send_command(cmd);
    DELAY_0001;
    uint8_t i = 0;
    for (i = 0; i < 16; i++) {
        lcd_send_data(' ');
        DELAY_0001;
    }
    lcd_send_command(cmd);
}
