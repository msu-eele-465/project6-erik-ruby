/**
* @file
* @brief Uses PWMs through 3 arbitrary outputs to change an LED's color
*
* Assumes all 3 ports are on the same base.
*/
#include <msp430.h>

#include "../src/led_status.h"

#include <stdio.h>

void init_LED(status_LED *sl)
{
     // Configure GPIO
    P6DIR |= sl->red_port_bit + sl->green_port_bit + sl->blue_port_bit;
    P6SEL0 |= sl->red_port_bit + sl->green_port_bit + sl->blue_port_bit;

    TB3CCR0 = 256-1;                          // PWM Period
    TB3CCTL1 = OUTMOD_7;                      // CCR1 reset/set (red)
    TB3CCTL2 = OUTMOD_7;                      // CCR2 reset/set (green)
    TB3CCTL3 = OUTMOD_7;                      // CCR3 reset/set (blue)
    // start locked
    TB3CCR1 = 255;                              // CCR1 PWM duty cycle: Amount of red 
    TB3CCR2 = 0;                               // CCR2 PWM duty cycle: Amount of green 
    TB3CCR3 = 0;                               // CCR3 PWM duty cycle: Amount of blue
    TB3CTL = TBSSEL__SMCLK | MC__UP | TBCLR;  // SMCLK, up mode, clear TBR

    PM5CTL0 &= ~LOCKLPM5;   // turn on GPIO 
}

void set_LED(status_LED *sl, LED_State s)
{
    sl->current_state = s;
    TB3CTL &= ~MC__UP;  // setting MC=0 to stop timer

   switch (s) 
   {
        case LEDLOCKED:        // set pwm to rgb(196 62 29) changed to look more red
            TB3CCR1 = 255;  // CCR1 PWM duty cycle: Amount of red 
            TB3CCR2 = 0;   // CCR2 PWM duty cycle: Amount of green 
            TB3CCR3 = 0;   // CCR3 PWM duty cycle: Amount of blue
            break;

        case MIDUNLOCK:     // rgb(196 146 29) changed to look more yellow
            TB3CCR1 = 220;  
            TB3CCR2 = 120;   
            TB3CCR3 = 0;   
            break;

        case LEDUNLOCKED:      // rgb(29 162 196)
            TB3CCR1 = 29;  
            TB3CCR2 = 162;   
            TB3CCR3 = 196;   
            break;

        case WINDOW_CHANGE:
            TB3CCR1 = 255;  
            TB3CCR2 = 153;   
            TB3CCR3 = 51;   
            break;
        // default to locked
        default:
            TB3CCR1 = 196;  // CCR1 PWM duty cycle: Amount of red 
            TB3CCR2 = 62;   // CCR2 PWM duty cycle: Amount of green 
            TB3CCR3 = 29;   // CCR3 PWM duty cycle: Amount of blue
            break;
    }
    TB3CTL |= MC__UP;  // start timer again in up mode
}
