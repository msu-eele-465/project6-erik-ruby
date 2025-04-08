/**
* @file
* @brief LED bar functionality
*
*/

#include <msp430fr2310.h>
#include <stdint.h>
#include <stdbool.h>

uint8_t recieved_pattern = 0;
uint8_t data_recieved_count = 0;
uint8_t data_received = 0;

int main(void)
{
    // Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    // I2C Setup


    UCB0CTLW0 &=~UCSWRST;                                 //clear reset register

    // 1. Put eUSCI_B0 into software reset
    UCB0CTLW0 |= UCSWRST;        // UCSWRST = 1 for eUSCI_B0 in SW reset

    // 2. Configure eUSCI_B0
    UCB0CTLW0 |= UCMODE_3;                //I2C slave mode, SMCLK
    UCB0I2COA0 = 0x0A | UCOAEN;           //SLAVE0 own address is 0x0A| enable

    // 3. Configure Ports as I2C
    P1SEL1 &= ~BIT3;            // P1.3 = SCL
    P1SEL0 |= BIT3;

    P1SEL1 &= ~BIT2;            // P1.2 = SDA
    P1SEL0 |= BIT2;

    // 4. Take eUSCI_B0 out of SW reset
    UCB0CTLW0 &= ~UCSWRST;

    // 5. Enable Interrupts
    UCB0IE |= UCRXIE0;          // Enable I2C Rx0 IRQ

     // Timer B0
    // Math: 1s = (1*10^-6)(D1)(D2)(25k)    D1 = 5, D2 = 8
    TB0CTL |= TBCLR;        // Clear timer and dividers
    TB0CTL |= TBSSEL__SMCLK;  // Source = SMCLK
    TB0CTL |= MC__UP;       // Mode UP
    TB0CTL |= ID__8;         // divide by 8 
    TB0EX0 |= TBIDEX__5;    // divide by 5 (100)

    TB0CCR0 = 25000;

    TB0CCTL0 &= ~CCIFG;     // Clear CCR0
    TB0CCTL0 |= CCIE;       // Enable IRQ

    // Port setup
    P1DIR |= 0b11110011;
    P2DIR |= 0b11000001;

    // LED Setup
    P2DIR |= BIT0;          // Config as Output
    P2OUT |= BIT0;          // turn on to start

    __enable_interrupt();       // Enable Maskable IRQs

    // Disable low-power mode / GPIO high-impedance
    PM5CTL0 &= ~LOCKLPM5;

    while (true)
    {
        
    }
}

/**
* Writes pattern to LED bar
*/
void write_to_bar()
{
    // P1.4-7 = bit 0-3, P1.1 = b4, P1.0 = b5, P2.7 = b6, P2.6 = b7
    if((BIT0 & recieved_pattern) == 0)   
    {
        P1OUT &= ~BIT4;
    }
    else
    {
        P1OUT |= BIT4;
    }
    if((BIT1 & recieved_pattern) == 0)
    {
        P1OUT &= ~BIT5;
    }
    else
    {
        P1OUT |= BIT5;
    }
    if((BIT2 & recieved_pattern) == 0)
    {
        P1OUT &= ~BIT6;
    }
    else
    {
        P1OUT |= BIT6;
    }
    if((BIT3 & recieved_pattern) == 0)
    {
        P1OUT &= ~BIT7;
    }
    else
    {
        P1OUT |= BIT7;
    }
    if((BIT4 & recieved_pattern) == 0)
    {
        P1OUT &= ~BIT1;
    }
    else
    {
        P1OUT |= BIT1;
    }
    if((BIT5 & recieved_pattern) == 0)
    {
        P1OUT &= ~BIT0;
    }
    else
    {
        P1OUT |= BIT0;
    }
    if((BIT6 & recieved_pattern) == 0)
    {
        P2OUT &= ~BIT7;
    }
    else
    {
        P2OUT |= BIT7;
    }
    if((BIT7 & recieved_pattern) == 0)
    {
        P2OUT &= ~BIT6;
    }
    else
    {
        P2OUT |= BIT6;
    }
    
}


//-- Interrupt Service Routines -----------------------

/**
* receiveData
*/
#pragma vector = EUSCI_B0_VECTOR
__interrupt void receive_data(void)
{
    switch(UCB0IV)             // determines which IFG has been triggered
    {
    case USCI_I2C_UCRXIFG0:                 // ID 0x16: Rx IFG
        data_received = 1;
        recieved_pattern = UCB0RXBUF;    // retrieve data
        write_to_bar();
        break;
    default:
        break;
    }

}
//----- end receiveData------------

/**
* Heartbeat LED
*/
#pragma vector = TIMER0_B0_VECTOR
__interrupt void heartbeat_LED(void)
{
    P2OUT ^= BIT0;          // P2.0 xOR
    if(data_received != 0)
    {
        // Math: .2s = (1*10^-6)(D1)(D2)(5k)    D1 = 5, D2 = 8
        TB0CCR0 = 5000;
        data_recieved_count++;
        
        if(data_recieved_count == 10)
        {
            data_received = 0;
            data_recieved_count = 0;
            TB0CCR0 = 25000;
        }
    }

    TB0CCTL0 &= ~CCIFG;     // clear flag
}
// ----- end heartbeatLED-----
