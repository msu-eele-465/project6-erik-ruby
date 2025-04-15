/**
* @file
* @brief Implements LED_status and keypad to operate a pattern-displaying LED bar
*
*/
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/keypad.h"
#include "../src/lcd.h"
#include "intrinsics.h"
#include "msp430fr2355.h"


uint8_t current_pattern = 0, tx_byte_cnt = 0, avg_temp_flag = 0, cur_sec_elapsed, cur_min_elapsed, ambient_mode = 0, read_temp_flag = 0, read_time_flag = 0, read_sec = 0, has_readt = 0;
const uint8_t LED_BAR_ADDR = 0x0A, LM92_ADDR = 0b01001000, RTC_ADDR = 0x68;
char cur_char, cur_state; 
float lm92_temp_float = 0, lm19_temp= 0;

// initialize temperature variables
unsigned int temp_buffer[9];        // maximum window is 9
unsigned int total = 0;             // walking total of buffer values
uint16_t lm92_temp = 0;
uint8_t current_idx = 0;            // index of newest recorded values
uint8_t window_size = 3;            // default window size
uint8_t adc_flag =0;                 

// global keypad and pk_attempt initialization
Keypad keypad = {
    .lock_state = LOCKED,                           // locked is 1
    .row_pins = {BIT3, BIT2, BIT1, BIT0},      // order is 5, 6, 7, 8
    .col_pins = {BIT4, BIT5, BIT2, BIT0},    // order is 1, 2, 3, 4
    .passkey = {'1','1','1','1'},
};

/**
* inits pattern transmit
*/
void transmit_pattern()
{
    UCB0TBCNT = 1;
    UCB0I2CSA = LED_BAR_ADDR;                            
    while (UCB0CTLW0 & UCTXSTP);                      // Ensure stop condition got sent
    UCB0CTLW0 |= UCTR | UCTXSTT;                      // I2C TX, start condition
}

/**
* resets the 
*/
void reset_time()
{
    UCB0TBCNT = 3;                    // send 3 0s (reg addr, sec, minutes)
    UCB0I2CSA = RTC_ADDR;                            
    while (UCB0CTLW0 & UCTXSTP);        // Ensure stop condition got sent
    UCB0CTLW0 |= UCTR | UCTXSTT;        // I2C TX, start condition
    int reset[] = {0,0,0};
    lcd_set_time(reset);
    cur_min_elapsed = 0;
    cur_sec_elapsed = 0;
}

/**
* sets the lcd mode
*/
void transmit_lcd_mode(uint8_t mode)
{
    send_lcd_mode(mode);
    reset_time();               // since we're switching mode, reset rtc time
}

/**
* sets the lcd time to the received time from the RTC
*/
void transmit_lcd_elapsed_time()
{
    uint8_t time_arr[3];
    // multiply minutes by 60 and add to seconds to get 3 digits
    int total_sec = cur_min_elapsed * 60;
    total_sec = total_sec + cur_sec_elapsed;

    time_arr[0] = total_sec / 100;
    time_arr[1] = (total_sec / 100) % 10;
    time_arr[2] = total_sec % 10;

    lcd_set_time(time_arr);
}

/**
* sets temperature variables to default values
* window size is user defined
*/
void change_n(uint8_t new_window_size)
{
    window_size = new_window_size;
    current_idx = 0;
    has_readt = 0;
}

/**
* Calculate average temperature
* send result to LCD
*/
void avg_temp(){
    // round to the nearest tenth
    int average = total / window_size;

    // convert to Voltage
    // x/2^12 = reading/3.3 (Digital Num/Scale = AV/Reference)
    float temp = 0;
    temp = (((float) average) / 4095.0 ) * 3.3;

    // convert voltage to temp in C
    temp = (temp - 1.3605) / (-11.77/1000.0);
    lm19_temp = temp;

    int int_arr[3];

    int_arr[0] = ((int) temp / 10);

    int_arr[1] = ((int) temp % 10);

    int_arr[2] = ((int)(temp * 10) % 10);

    set_temperature_ambient(int_arr);
    
}

/**
* initializes LED 1, Timers, and LED bar ports
* 
* @param: NA
*
* @return: NA
*/
void init(void)
{

    // Disable watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

//------------- Setup Ports --------------------
    // LED1
    P1DIR |= BIT0;              // Config as Output
    P1OUT |= BIT0;              // turn on to start
    // LED2
    P6DIR |= BIT6;              // Config as Output
    P6OUT |= BIT6;              // turn on to start

    // Configure Peltier Device Pins: 6.0 is heat, 6.1 is cool.
    P6DIR |= BIT0 + BIT1;
    P6OUT &= ~(BIT1 + BIT0);    // Start off... IMPORTANT!!!!!!!!!!!


    // Timer B0
    // Math: 1s = (1*10^-6)(D1)(D2)(25k)    D1 = 5, D2 = 8
    TB0CTL |= TBCLR;            // Clear timer and dividers
    TB0CTL |= TBSSEL__SMCLK;    // Source = SMCLK
    TB0CTL |= MC__UP;           // Mode UP
    TB0CTL |= ID__8;            // divide by 8 
    TB0EX0 |= TBIDEX__5;        // divide by 5 (100)

    TB0CCR0 = 25000;

    TB0CCTL0 &= ~CCIFG;         // Clear CCR0
    TB0CCTL0 |= CCIE;           // Enable IRQ

    // Timer B1
    // Math: .5s = (1*10^-6)(D1)(D2)(25k)    D1 = 5, D2 = 4
    TB1CTL |= TBCLR;            // Clear timer and dividers
    TB1CTL |= TBSSEL__SMCLK;    // Source = SMCLK
    TB1CTL |= MC__UP;           // Mode UP
    TB1CTL |= ID__4;            // divide by 4 
    TB1EX0 |= TBIDEX__5;        // divide by 5 (100)

    TB1CCR0 = 25000;

    TB1CCTL0 &= ~CCIFG;         // Clear CCR0
    TB1CCTL0 |= CCIE;           // Enable IRQ

     // Configure Pins for I2C
    P1SEL1 &= ~BIT3;            // P1.3 = SCL
    P1SEL1 &= ~BIT2;            // P1.2 = SDA
    P1SEL0 |= BIT2 | BIT3;                            // I2C pins

    // Configure USCI_B0 for I2C mode
    UCB0CTLW0 |= UCSWRST;                             // put eUSCI_B in reset state
    UCB0CTLW0 |= UCMODE_3 | UCMST;                    // I2C master mode, SMCLK
    UCB0CTLW1 |= UCASTP_2;                            // Automatic stop after hit TBCNT
    UCB0I2CSA = LED_BAR_ADDR;                         // configure slave address
    UCB0BRW = 0x8;                                    // baudrate = SMCLK / 8
    UCB0TBCNT = 2;                                    // num bytes to recieve

    UCB0CTLW0 &=~ UCSWRST;                            // clear reset register
    UCB0IE |= UCTXIE0 | UCNACKIE | UCRXIE0 | UCBCNTIE;// transmit, receive, TBCNT, and NACK
    
    //--- Configure ADC
    ADCCTL0 &= ~ADCSHT;         // Clear ADCSHT from def. of ADCSHT = 01
    ADCCTL0 |= ADCSHT_2;        // Conversion Cycles = 16 (ADCSHT = 10)
    ADCCTL0 |= ADCON;           // Turn ADC ON

    ADCCTL1 |= ADCSSEL_2;       // ADC Clock Source = SMCLK
    ADCCTL1 |= ADCSHP;          // Sample signal source = sampling timer

    ADCCTL2 &= ~ADCRES;         // Clear ADCRES from def. of ADCRES=01
    ADCCTL2 |= ADCRES_2;        // Resolution = 12-bit (ADCRES = 10)

    ADCMCTL0 |= ADCINCH_1;      // ADC Input Channel = A2 (P1.1): sends A2 to ADC

    ADCIE |= ADCIE0;            // Enable ADC Conv Complete IRQ

//------------- END PORT SETUP -------------------

    PM5CTL0 &= ~LOCKLPM5;   // turn on GPIO
    __enable_interrupt();   // enable maskable IRQs
}

void set_state(char state)
{
    cur_state = state;
    switch(cur_char)   
    {
        case HEAT:                      // set heat pin to 1, cool pin to 0
            P6OUT |= BIT0;
            P6OUT &= ~BIT1;
            current_pattern = 2;
            break;
        case COOL:                      // set heat pin to 0, cool pin to 1
            P6OUT |= BIT0;
            P6OUT &= ~BIT1;
            current_pattern = 1;
            break;
        case OFF:                       // set pins to be both 0 V
            if(ambient_mode)
            {
                ambient_mode = 0;
            }
            current_pattern = 0;
            P6OUT &= ~(BIT1 + BIT0);
            break;
        default:
            
            break;
    }
    transmit_pattern();
}


/**
* Handle character reading and set base multiplier, setting patterns as well
* 
* @param: NA
*
* @return: NA
*/
int main(void)
{
    cur_char = 'Z';
    int ret = FAILURE;

    init();
    init_keypad(&keypad);
    set_state(OFF);
    transmit_lcd_mode(3);                    // resets time too

    while(1)
    {
        ret = scan_keypad(&keypad, &cur_char);
        __delay_cycles(100000);             // Delay for 100000*(1/MCLK)=0.1s
        if (ret == SUCCESS)
        {
            switch(cur_char)
            {
                case HEAT:
                    if((cur_state != HEAT) || (ambient_mode == 1))
                    {
                        ambient_mode = 0;
                        set_state(HEAT);
                        transmit_lcd_mode(0);
                    }
                    break;
                case COOL:
                    if((cur_state != COOL) || (ambient_mode == 1))
                    {
                        ambient_mode = 0;
                        set_state(COOL);
                        transmit_lcd_mode(1);
                    }
                    break;
                case AMBIENT:
                    if(ambient_mode == 0)
                    {
                        ambient_mode = 1;
                        transmit_lcd_mode(2);
                    }
                    break;
                case OFF:
                    if(cur_state!= OFF)
                    {
                        set_state(OFF);
                        transmit_lcd_mode(3);
                    }
                    break;
                default:
                    break;
            }
        }

        if(ambient_mode)
        {
            // if cooler than ambient, set to heating mode.
            // tolerance of +/- 2 celsius
            if(lm92_temp_float < lm19_temp - 2)
            {
                set_state(HEAT);
            }
            // if warmer than ambient, set to cooling mode.
            else if (lm92_temp_float > lm19_temp + 2)
            {
                set_state(COOL);
            }            
        }

        if(avg_temp_flag)
        {
            avg_temp();
            avg_temp_flag = 0;
        }

        if(adc_flag)
        { 
            ADCCTL0 |= ADCENC | ADCSC;
            adc_flag = 0;
        }

        // read temperature from LM92
        if(read_temp_flag)
        {
            while (UCB0CTLW0 & UCTXSTP);                      // Ensure stop condition got sent
            UCB0TBCNT = 2;
            UCB0I2CSA = LM92_ADDR;

            UCB0CTLW0 &= ~UCTR;          // Put into Rx mode
            UCB0CTLW0 |= UCTXSTT;        // generate START cond.

            read_temp_flag = 0;

            lm92_temp_float = ((float)lm92_temp) * .0625;

            int int_arr[3];
            int_arr[0] = ((int) lm92_temp_float / 10);
            int_arr[1] = ((int) lm92_temp_float % 10);
            int_arr[2] = ((int)(lm92_temp_float * 10) % 10);
            set_temperature_plant(int_arr);

            lm92_temp = 0;
        }

        // send register address of time, read 
        if(read_time_flag)
        {
            UCB0TBCNT = 1;
            UCB0I2CSA = RTC_ADDR;
            while (UCB0CTLW0 & UCTXSTP);                      // Ensure stop condition got sent
            UCB0CTLW0 |= UCTR | UCTXSTT;                      // I2C TX, start condition
            __delay_cycles(1000);
            while (UCB0CTLW0 & UCTXSTP);                      // Ensure stop condition got sent
            UCB0TBCNT = 2;

            UCB0CTLW0 &= ~UCTR;          // Put into Rx mode
            UCB0CTLW0 |= UCTXSTT;        // generate START cond.

            read_time_flag = 0;
            
            if(cur_min_elapsed > 4)          // after 300s (5 min), turn off
            {
                __delay_cycles(1000);
                set_state(OFF);
                transmit_lcd_mode(3);
            }
            transmit_lcd_elapsed_time();
        }

        __delay_cycles(100000);             // Delay for 100000*(1/MCLK)=0.1s
    }

    return(0);
}


//-- Interrupt Service Routines -----------------------

/**
* transmit_data
*/
#pragma vector = EUSCI_B0_VECTOR
__interrupt void transmit_data(void)
{
    switch(UCB0IV)             // determines which IFG has been triggered
    {
    case USCI_I2C_UCNACKIFG:
        UCB0CTL1 |= UCTXSTT;                      //resend start if NACK
        break;                                      // Vector 4: NACKIFG break
    case USCI_I2C_UCTXIFG0:
        if(UCB0I2CSA == LED_BAR_ADDR)
        {
            UCB0TXBUF = current_pattern;            // Load TX buffer
        }
        else if(UCB0I2CSA == RTC_ADDR)
        {
            // register address begins at 0, then we set sec and min to 0
            UCB0TXBUF = 0;                          
        }
        else        // LM92
        {
        
        }
        break;
    case USCI_I2C_UCRXIFG0:                 // ID 0x16: Rx IFG
        if(UCB0I2CSA == LM92_ADDR)
        {
            if(lm92_temp != 0)
            {
                lm92_temp |= UCB0RXBUF;
            }
            else 
            {
                lm92_temp = UCB0RXBUF;
                lm92_temp <<= 4;
            }
        }
        else 
        {
            if(read_sec)
            {
                cur_min_elapsed = UCB0RXBUF; 
                read_sec = 0;   
            }
            else 
            {
                cur_sec_elapsed = UCB0RXBUF;
                ++read_sec;
            }
        }
        break;                                    
    default:
        break;
    }

}


/**
* Heartbeat LED, read time every 1s
*/
#pragma vector = TIMER0_B0_VECTOR
__interrupt void heartbeat_LED(void)
{
    P1OUT ^= BIT0;          // LED1 xOR
    if(cur_state != OFF)
    {
        read_time_flag = 1;
    }
    TB1CCTL0 &= ~CCIFG;     // clear flag
}
// ----- end heartbeat_LED-----

/**
* read from ADC and LM92 every .5s
*/
#pragma vector = TIMER1_B0_VECTOR
__interrupt void read_temps(void)
{
    P6OUT ^= BIT6;
    adc_flag = 1;
    read_temp_flag = 1;
    TB1CCTL1 &= ~CCIFG;     // clear flag
}

/**
* Read temperature value from ADC
*/
#pragma vector = ADC_VECTOR
__interrupt void record_av(void)
{
    // save to current index
    temp_buffer[current_idx] = ADCMEM0;
    ++current_idx;
    if((current_idx == window_size) && (has_readt == 0))
    {
        current_idx = 0;
        total = 0;
        
        uint8_t i;
        for(i = 0; i < window_size; ++i)
        {
            total += temp_buffer[i];
        }
        avg_temp_flag = 1;
    }
    else if (has_readt == 1) {
        total = 0;
        uint8_t i;
        for(i = 0; i < window_size; ++i)
        {
            total += temp_buffer[i];
        }
        avg_temp_flag = 1;

        if (current_idx == window_size) {
            current_idx = 0;
        }
    }
}