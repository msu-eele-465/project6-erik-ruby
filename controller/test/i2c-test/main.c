/**
* @file
* @brief Implements LED_status and keypad to operate a pattern-displaying LED bar
*
*/
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "intrinsics.h"
#include "msp430fr2355.h"


uint8_t current_pattern = 0, tx_byte_cnt = 0, avg_temp_flag = 0, cur_sec_elapsed, cur_min_elapsed, ambient_mode = 0, read_temp_flag = 0, read_time_flag = 0, read_sec = 0;
const uint8_t LED_BAR_ADDR = 0x0A, LM92_ADDR = 0b01001000, RTC_ADDR = 0x68;
char cur_char, cur_state; 

// initialize temperature variables
unsigned int temp_buffer[9];        // maximum window is 9
unsigned int total = 0;             // walking total of buffer values
uint8_t current_idx = 0;            // index of newest recorded values
uint8_t window_size = 3;            // default window size
uint8_t adc_flag =0;             

/**
* resets the time
*/
void reset_time()
{
    UCB0TBCNT = 3;                   // send 3 0s (reg addr, sec, minutes)
    // UCB0I2CSA = RTC_ADDR;                            
    while (UCB0CTLW0 & UCTXSTP);        // Ensure stop condition got sent
    UCB0CTLW0 |= UCTR | UCTXSTT;        // I2C TX, start condition
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
    UCB0CTLW1 |= UCASTP_2;                            // Automatic stop after hit TBCNT
    UCB0I2CSA = RTC_ADDR;                         // configure slave address
    UCB0BRW = 0x8;                                    // baudrate = SMCLK / 8
    UCB0TBCNT = 2;                                    // num bytes to recieve

    UCB0CTLW0 &=~ UCSWRST;                            // clear reset register
    // UCB0IE |= UCTXIE0 | UCNACKIE;
    UCB0IE |= UCTXIE0 | UCNACKIE | UCRXIE0 | UCBCNTIE;// transmit, receive, TBCNT, and NACK
//------------- END PORT SETUP -------------------

    __enable_interrupt();   // enable maskable IRQs
    PM5CTL0 &= ~LOCKLPM5;   // turn on GPIO


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

    init();
    reset_time();


    while(1)
    {
        // send register address of time, read 
        if(read_time_flag)
        {
            ++total;
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
            
            if(total == 100)          // every 100s, "change mode"
            {
                 __delay_cycles(1000);
                reset_time();
                total = 0;
            }
        }

        // if(send_time_flag)
        // {
        //     reset_time();
        //     send_time_flag = 0;
        // }

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
        // register address begins at 0, then we set sec and min to 0
        UCB0TXBUF = 0;                          
        break;   
    case USCI_I2C_UCRXIFG0:                 // ID 0x16: Rx IFG
        if(read_sec)
        {
            cur_min_elapsed = UCB0RXBUF; 
            read_sec = 0;
            // UCB0CTLW0 |= UCTXSTP;                     // I2C stop condition
            // UCB0IFG &= ~UCRXIFG;                      // Clear USCI_B0 RX int    
        }
        else 
        {
            cur_sec_elapsed = UCB0RXBUF;
            ++read_sec;
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
    read_time_flag = 1;
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
    // adc_flag = 1;
    TB1CCTL1 &= ~CCIFG;     // clear flag
}
