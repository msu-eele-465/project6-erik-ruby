/**
* @file
* @brief Implements LED_status and keypad to operate a pattern-displaying LED bar
*
*/
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "lcd.h"
#include "intrinsics.h"
#include "msp430fr2355.h"

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


/**
* Handle character reading and set base multiplier, setting patterns as well
* 
* @param: NA
*
* @return: NA
*/
int main(void)
{
    while(1)
    {
        send_lcd_mode(0);
        __delay_cycles(500000);             // Delay for 100000*(1/MCLK)=0.1s
        send_lcd_mode(1);
        __delay_cycles(500000);             // Delay for 100000*(1/MCLK)=0.1s
        send_lcd_mode(2);
        __delay_cycles(500000);             // Delay for 100000*(1/MCLK)=0.1s
        send_lcd_mode(3);
        __delay_cycles(500000);             // Delay for 100000*(1/MCLK)=0.1s
    }

    return(0);
}


//-- Interrupt Service Routines -----------------------


/**
* Heartbeat LED, read time every 1s
*/
#pragma vector = TIMER0_B0_VECTOR
__interrupt void heartbeat_LED(void)
{
    P1OUT ^= BIT0;          // LED1 xOR
    TB1CCTL0 &= ~CCIFG;     // clear flag
}
// ----- end heartbeat_LED-----
