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


uint8_t tx_byte_cnt = 0;
const uint8_t LCD_ADDR = 0x0B;
char cur_char;
// initialize temperature variables
unsigned int temp_buffer[9] = {0};        // maximum window is 9
unsigned int total = 0;             // walking total of buffer values
uint8_t oldest_idx, current_idx;    // index of oldest and newest recorded values
uint8_t init_count = 1;                   // is the first value being received?
uint8_t window_size = 3;            // default window size
uint8_t units = 0;                     // 0 is Celsius, 1 is fahrenheit
uint8_t avg_temp_flag = 0; 

void reset_temp(){
    oldest_idx = 0;
    current_idx = 0;
    init_count = 1;
    total = 0;
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
* initializes LED 1, Timers, and LED bar ports
* 
* @param: NA
*
* @return: NA
*/
void init(void)
{
    tx_byte_cnt = 0;

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
    // Math: .5s = (1*10^-6)(D1)(D2)(25k)    D1 = 5, D2 = 4
    TB1CTL |= TBCLR;            // Clear timer and dividers
    TB1CTL |= TBSSEL__SMCLK;    // Source = SMCLK
    TB1CTL |= MC__UP;           // Mode UP
    TB1CTL |= ID__4;            // divide by 4 
    TB1EX0 |= TBIDEX__5;        // divide by 5 (100)

    TB1CCR0 = 50000;

    TB1CCTL0 &= ~CCIFG;         // Clear CCR0
    TB1CCTL0 |= CCIE;           // Enable IRQ


     // Configure Pins for I2C
    P1SEL1 &= ~BIT3;            // P1.3 = SCL
    P1SEL1 &= ~BIT2;            // P1.2 = SDA
    P1SEL0 |= BIT2 | BIT3;                            // I2C pins

    // Configure USCI_B0 for I2C mode
    UCB0CTLW0 |= UCSWRST;                             // put eUSCI_B in reset state
    UCB0CTLW0 |= UCMODE_3 | UCMST;                    // I2C master mode, SMCLK
    UCB0I2CSA = LCD_ADDR;                         // configure slave address
    UCB0BRW = 0x8;                                    // baudrate = SMCLK / 8

    UCB0CTLW0 &=~ UCSWRST;                            // clear reset register
    UCB0IE |= UCTXIE0 | UCNACKIE;                     // transmit and NACK interrupt enable

     //--- Configure ADC
    // ADCCTL0 &= ~ADCSHT;         // Clear ADCSHT from def. of ADCSHT = 01
    // ADCCTL0 |= ADCSHT_2;        // Conversion Cycles = 16 (ADCSHT = 10)
    // ADCCTL0 |= ADCON;           // Turn ADC ON

    // ADCCTL1 |= ADCSSEL_2;       // ADC Clock Source = SMCLK
    // ADCCTL1 |= ADCSHP;          // Sample signal source = sampling timer

    // ADCCTL2 &= ~ADCRES;         // Clear ADCRES from def. of ADCRES=01
    // ADCCTL2 |= ADCRES_2;        // Resolution = 12-bit (ADCRES = 10)

    // ADCMCTL0 |= ADCINCH_2;      // ADC Input Channel = A2 (P1.2): sends A2 to ADC

    // ADCIE |= ADCIE0;            // Enable ADC Conv Complete IRQ
    
//------------- END PORT SETUP -------------------

    reset_temp();

    __enable_interrupt();   // enable maskable IRQs
    PM5CTL0 &= ~LOCKLPM5;   // turn on GPIO

}

/**
* Calculate average temperature
* send result over I2C
*/
void avg_temp(){
//     // round to the nearest tenth
//     int average = total / window_size;

//     // convert to Voltage
//     // x/2^12 = reading/3.3 (Digital Num/Scale = AV/Reference)
//     float temp = ((float) average) / 4095.0 * 3.3;

//     // convert voltage to temp in C
//     temp = (temp - 1.8605) / (-11.77/100.0);

//     // convert to fahrenheit?
//     if (units) {
//         temp = (temp * (9.0/5.0)) + 32.0;
//     }

//     char char_arr[3];

    // char_arr[0] = ((int) temp / 10) + '0';
    // transmit_to_lcd();

    // char_arr[1] = ((int) temp % 10) + '0';

    // char_arr[2] = ((int)(temp * 10) % 10) + '0';
    cur_char = '1';
    tx_byte_cnt = 1;
    UCB0I2CSA = LCD_ADDR;
    while (UCB0CTLW0 & UCTXSTP);                      // Ensure stop condition got sent
    UCB0CTLW0 |= UCTR | UCTXSTT;                      // I2C TX, start condition
    __delay_cycles(1000);

   cur_char = '2';
    tx_byte_cnt = 1;
    UCB0I2CSA = LCD_ADDR;
    while (UCB0CTLW0 & UCTXSTP);                      // Ensure stop condition got sent
    UCB0CTLW0 |= UCTR | UCTXSTT;                      // I2C TX, start condition
    __delay_cycles(1000);

    cur_char = '3';
    tx_byte_cnt = 1;
    UCB0I2CSA = LCD_ADDR;
    while (UCB0CTLW0 & UCTXSTP);                      // Ensure stop condition got sent
    UCB0CTLW0 |= UCTR | UCTXSTT;                      // I2C TX, start condition
    __delay_cycles(1000);

    // int i;
    // for (i = 0; i < 3; i++){
    //     cur_char = char_arr[i];
    //     transmit_to_lcd();
    //     __delay_cycles(1000);
    // }
    
}


/**
* Handle character reading and set base multiplier, setting patterns as well
*/
int main(void)
{
    init(); 
    while(true)
    {
        // __delay_cycles(1000000);
        // avg_temp();
        if(avg_temp_flag)
        {
            avg_temp();
            avg_temp_flag = 0;
        }
        
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
            UCB0TXBUF = cur_char;           
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
    avg_temp_flag = 1;
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
    // unsigned int ADC_Value = ADCMEM0;                // Read ADC result
 
    // if (current_idx == oldest_idx) {
    //     // overwriting the oldest index, subtract from total before saving
    //     total -= temp_buffer[oldest_idx];
    //     oldest_idx = (oldest_idx + 1) % window_size;
    // }
    
    // // save to current index
    // temp_buffer[current_idx] = ADC_Value;
    // total += ADC_Value;
    
    current_idx = (current_idx + 1) % window_size;

    if (!init_count) {
        // avg_temp();
    } else if (init_count && (current_idx == (window_size - 1))){
        // wait to send average temp
        init_count = 0;
    }

    TB1CCTL1 &= ~CCIFG;     // clear flag
}