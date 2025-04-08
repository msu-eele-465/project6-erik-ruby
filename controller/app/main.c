/**
* @file
* @brief Implements LED_status and keypad to operate a pattern-displaying LED bar
*
*/
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/keypad.h"
#include "../src/led_status.h"
#include "intrinsics.h"
#include "msp430fr2355.h"


uint8_t current_pattern = 0, led3_counter = 0, tx_byte_cnt = 0, reset_counter = 0, avg_temp_flag = 0, current_pattern_state = 9;
// starting values for every pattern
const uint8_t DEFAULT_PATTERNS[8] = {170, 170, 0, 24, 255, 1, 127, 1};
const uint8_t LED_BAR_ADDR = 0x0A, LCD_ADDR = 0x0B;
uint8_t patterns[8] = {170, 170, 0, 24, 255, 1, 127, 1};
char cur_char;

// initialize temperature variables
unsigned int temp_buffer[9] = {0};        // maximum window is 9
unsigned int total = 0;             // walking total of buffer values
uint8_t oldest_idx, current_idx;    // index of oldest and newest recorded values
uint8_t prev_idx = 0;
uint8_t init_count = 1;                   // is the first value being received?
uint8_t window_size = 3;            // default window size
uint8_t units = 0, adc_flag =0;                     // 0 is Celsius, 1 is fahrenheit

// global keypad and pk_attempt initialization
Keypad keypad = {
    .lock_state = LOCKED,                           // locked is 1
    .row_pins = {BIT3, BIT2, BIT1, BIT0},      // order is 5, 6, 7, 8
    .col_pins = {BIT4, BIT5, BIT2, BIT0},    // order is 1, 2, 3, 4
    .passkey = {'1','1','1','1'},
};

char pk_attempt[4] = {'x','x','x','x'};

const uint8_t LED3_PATTERN[6] = {
    0b00011000,  // Step 0
    0b00100100,  // Step 1
    0b01000010,  // Step 2
    0b10000001,  // Step 3
    0b01000010,  // Step 4
    0b00100100   // Step 5
};

// PERSISTENT stores vars in FRAM so they stick around through power cycles
__attribute__((persistent)) static status_LED status_led =
{
    .led_port_base_addr = P5_BASE,
    .red_port_bit = BIT0,
    .green_port_bit = BIT1,
    .blue_port_bit = BIT2,
    .current_state = LEDLOCKED
};

/**
* inits pattern transmit
*/
void transmit_pattern()
{
    tx_byte_cnt = 1;
    UCB0I2CSA = LED_BAR_ADDR;                            
    while (UCB0CTLW0 & UCTXSTP);                      // Ensure stop condition got sent
    UCB0CTLW0 |= UCTR | UCTXSTT;                      // I2C TX, start condition
}

/**
* inits lcd transmit
*/
void transmit_to_lcd()
{
    tx_byte_cnt = 1;
    UCB0I2CSA = LCD_ADDR;
    while (UCB0CTLW0 & UCTXSTP);                      // Ensure stop condition got sent
    UCB0CTLW0 |= UCTR | UCTXSTT;                      // I2C TX, start condition
}

/**
* sets temperature variables to default values
* window size is user defined
*/
void reset_temp(){
    oldest_idx = 0;
    current_idx = 0;
    init_count = 1;
    total = 0;
}

/**
* Calculate average temperature
* send result over I2C
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

    // convert to fahrenheit?
    if (units) {
        temp = (temp * (9.0/5.0)) + 32.0;
    }

    char char_arr[3];

    char_arr[0] = ((int) temp / 10) + '0';

    char_arr[1] = ((int) temp % 10) + '0';

    char_arr[2] = ((int)(temp * 10) % 10) + '0';

    int i;
    for (i = 0; i < 3; i++){
        cur_char = char_arr[i];
        transmit_to_lcd();
        __delay_cycles(1000);
    }
    
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
    // Math: 1s = (1*10^-6)(D1)(D2)(25k)    D1 = 5, D2 = 4
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
    UCB0I2CSA = LED_BAR_ADDR;                         // configure slave address
    UCB0BRW = 0x8;                                    // baudrate = SMCLK / 8

    UCB0CTLW0 &=~ UCSWRST;                            // clear reset register
    UCB0IE |= UCTXIE0 | UCNACKIE;                     // transmit and NACK interrupt enable
    
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

    __enable_interrupt();   // enable maskable IRQs
    PM5CTL0 &= ~LOCKLPM5;   // turn on GPIO

    init_LED(&status_led);
    reset_temp();
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
    int count = 0;

    init();
    init_keypad(&keypad);
    current_pattern = 0;
    transmit_pattern();

    while(true)
    {
        ret = scan_keypad(&keypad, &cur_char);
        __delay_cycles(100000);             // Delay for 100000*(1/MCLK)=0.1s
        if (ret == SUCCESS)
        {
            if (cur_char == 'D')
            {
                current_pattern = 0;
                transmit_pattern();
                __delay_cycles(1000);
                transmit_to_lcd();
                set_lock(&keypad, LOCKED);
                set_LED(&status_led, LEDLOCKED);
                reset_pk(pk_attempt);
                count = 0;
                reset_counter = 0;
            } 
            else if (keypad.lock_state == LOCKED)  // if we're locked and trying to unlock
            {
                reset_counter = 0;
                current_pattern = 0;
                current_pattern_state = 9;
                pk_attempt[count] = cur_char;
                count++;
                if (count == 4)
                {
                    if(check_status(&keypad, pk_attempt) == SUCCESS)
                    {
                        set_lock(&keypad, UNLOCKED);
                        set_LED(&status_led, LEDUNLOCKED);
                    }
                    count = 0;
                }
                else if (count > 0)
                {
                    set_LED(&status_led, MIDUNLOCK);
                }
            }
            else if(status_led.current_state == PATTERN_CHANGE)           // in pattern_state so do pattern stuff. use p3.0-3.7, and 6.4, 6.5 for MSB
            {
                char temp_char = cur_char;
                cur_char = 'G';
                transmit_to_lcd();
                __delay_cycles(1000);
                cur_char = temp_char;
                transmit_to_lcd();
                set_LED(&status_led, LEDUNLOCKED);
                switch(cur_char)
                {
                    case '0': // put pattern 0 on ports

                        if(current_pattern_state != 0)
                        {
                            current_pattern_state = 0;
                        }
                        else        // pattern selected twice in a row, reset
                        {
                            patterns[0] = DEFAULT_PATTERNS[0];
                        }
                        break;
                    case '1': // put pattern 1 on ports

                        if(current_pattern_state != 1)
                        {
                            current_pattern_state = (1);
                        }
                        else
                        {
                            patterns[1] = DEFAULT_PATTERNS[1];
                        }
                        break;

                    case '2': // put pattern 2 on ports

                        if(current_pattern_state != 2)
                        {
                            current_pattern_state = (2);
                        }
                        else
                        {
                            patterns[2] = DEFAULT_PATTERNS[2];
                        }
                        break;

                    case '3': // put pattern 3 on ports

                        if(current_pattern_state != 3)
                        {
                            current_pattern_state = (3);
                        }
                        else
                        {
                            patterns[3] = DEFAULT_PATTERNS[3];
                        }
                        break;
                    case '4': // put pattern 4 on ports

                        if(current_pattern_state != 4)
                        {
                            current_pattern_state = (4);
                        }
                        else
                        {
                            patterns[4] = DEFAULT_PATTERNS[4];
                        }
                        break;
                    case '5': // put pattern 5 on ports
                        if(current_pattern_state != 5)
                        {
                            current_pattern_state = (5);
                        }
                        else
                        {
                            patterns[5] = DEFAULT_PATTERNS[5];
                        }
                        break;
                    case '6': // put pattern 6 on ports

                        if(current_pattern_state != 6)
                        {
                            current_pattern_state = (6);
                        }
                        else
                        {
                            patterns[6] = DEFAULT_PATTERNS[6];
                        }
                        break;
                    case '7': // put pattern 7 on ports

                        if(current_pattern_state != 7)
                        {
                            current_pattern_state = (7);
                        }
                        else
                        {
                            patterns[7] = DEFAULT_PATTERNS[7];
                        }
                        break;

                    default:
                        break;
                } 
            }
            else if (status_led.current_state == WINDOW_CHANGE) {
                char temp_char = cur_char;
                cur_char = 'G';
                transmit_to_lcd();
                __delay_cycles(1000);
                cur_char = temp_char;
                transmit_to_lcd();
                window_size = (cur_char - '0');
                reset_temp();
                set_LED(&status_led, LEDUNLOCKED);
            }
            else if (keypad.lock_state == UNLOCKED) 
            {
                switch(cur_char)
                {
                    case 'A':
                        set_LED(&status_led, WINDOW_CHANGE); 
                        transmit_to_lcd();
                        break;
                    case 'B':
                        set_LED(&status_led, PATTERN_CHANGE);
                        transmit_to_lcd();
                        break;
                    case 'C':
                        units ^= 1;
                        transmit_to_lcd();
                        break;
                    default:
                        break;   
                }
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
        if (tx_byte_cnt)                                // Check TX byte counter
        {
            if(UCB0I2CSA == LED_BAR_ADDR)
            {
                UCB0TXBUF = current_pattern;            // Load TX buffer
            }
            else 
            {
                UCB0TXBUF = cur_char;           
            }
            tx_byte_cnt--;                              // Decrement TX byte counter
        }
        else
        {
            UCB0CTLW0 |= UCTXSTP;                     // I2C stop condition
            UCB0IFG &= ~UCTXIFG;                      // Clear USCI_B0 TX int flag
        } 
        break;                                    
    default:
        break;
    }

}


/**
* Heartbeat LED
*/
#pragma vector = TIMER0_B0_VECTOR
__interrupt void heartbeat_LED(void)
{
    P1OUT ^= BIT0;          // LED1 xOR
    reset_counter++;
    if (reset_counter >= 5 && status_led.current_state == MIDUNLOCK){
        current_pattern = 0;
        set_lock(&keypad, LOCKED);
        set_LED(&status_led, LEDLOCKED);
        reset_pk(pk_attempt);
        reset_counter = 0;
    }

    switch(current_pattern_state)
    {
        case 0:
            current_pattern = patterns[0];
            break;
        case 1: 
            patterns[1] ^= 0xFF;    // flip every bit
            current_pattern = patterns[1];
            break;
        case 2: 
            patterns[2] += 1;    // up counter
            current_pattern = patterns[2];
            break;
       case 3:           // in and out
            patterns[3] = LED3_PATTERN[led3_counter];
            ++led3_counter;
            if(led3_counter == 6)
            {
                led3_counter = 0;
            }
            current_pattern = patterns[3];
            break;
        case 4: 
            patterns[4] -= 1;    // down counter
            current_pattern = patterns[4];
            break;
        case 5:          // rotate left
            if(patterns[5] == 0b10000000)
            {
                patterns[5] = DEFAULT_PATTERNS[5];
            }
            else 
            {
                patterns[5] <<= 1;    
            }
            current_pattern = patterns[5];
            break;
        case 6:          // rotate right
            if(patterns[6] == 0b11111110)
            {
                patterns[6] = DEFAULT_PATTERNS[6];
            }
            else 
            {
                patterns[6] >>= 1;  
                patterns[6] += 128;     // 0b1000 0000 = 128, adds a 1 at msb
            } 
            current_pattern = patterns[6];
            break;
        case 7:          // fill to the left
            if(patterns[7] == 0b11111111)
            {
                patterns[7] = DEFAULT_PATTERNS[7];
            }
            else 
            {
                patterns[7] <<= 1; 
                patterns[7] += 1;   
            }
            current_pattern = patterns[7];
            break;
        default:
            current_pattern = 0;
            break;
    }
    if(current_pattern != 0)
    {
        transmit_pattern();
    }

    TB1CCTL0 &= ~CCIFG;     // clear flag
}
// ----- end heartbeat_LED-----

/**
* read from ADC every .5s
*/
#pragma vector = TIMER1_B0_VECTOR
__interrupt void read_ADC(void)
{
    P6OUT ^= BIT6;
    adc_flag = 1;
     if (current_idx == oldest_idx) {
        // overwriting the oldest index, subtract from total before saving
        total -= temp_buffer[oldest_idx];
        oldest_idx = (oldest_idx + 1) % window_size;
    }

    prev_idx = current_idx;
    current_idx = (current_idx + 1) % window_size;

    if (!init_count) {
       avg_temp_flag = 1;
    } else if (init_count && (current_idx == (window_size - 1))){
        // wait to send average temp
        init_count = 0;
    }
    TB1CCTL1 &= ~CCIFG;     // clear flag
}

/**
* Read temperature value from ADC
*/
#pragma vector = ADC_VECTOR
__interrupt void recordAV(void)
{
    // save to current index
    temp_buffer[prev_idx] = ADCMEM0;
    total += ADCMEM0;
    
}