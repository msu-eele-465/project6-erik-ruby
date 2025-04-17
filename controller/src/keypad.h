/**
* @file
* @brief Header file for keypad reading and password management
*
* Uses PWMs through 3 arbitrary RGB outputs to change an LED's color
*/
#ifndef  KEYPAD_H
#define KEYPAD_H

#include <msp430.h>
#include <stdio.h>

#ifndef LOCKED
#define LOCKED 1
#endif
#ifndef UNLOCKED
#define UNLOCKED 0
#endif
#ifndef SUCCESS
#define SUCCESS 1
#endif
#ifndef FAILURE
#define FAILURE 0
#endif

/**
* row and column pins, passkey, and current state
*/
typedef struct {
    int lock_state;
    int row_pins[4];      // order is 5, 6, 7, 8
    int col_pins[4];      // order is 1, 2, 3, 4
    char passkey[4];
} Keypad;

extern char key_chars [4][4];

/**
* initializes keypad GPIO pins
* 
* @param: keypad constructor.
*
* @return: NA
*/
void init_keypad(Keypad *keypad);
/**
* set state to locked or unlocked depending on a given char value
* 
* @param: keypad constructor.
* @param: current state.
*
* @return: NA
*/
void set_lock(Keypad *keypad, int lock);
/**
* check for a button press by setting a column low, then checking which row is also low
* 
* @param: keypad constructor.
* @param: pointer to pressed key.
*
* @return: SUCCESS or FAILURE
*/
int scan_keypad(Keypad *keypad, char *key_press);
/**
* compare the keypad passkey to the user's guess
* 
* @param: passkey
* @param: user guess
*
* @return: NA
*/
int compare_pw(char passkey[], char guess[]);
/**
* checks to see if a correct guess had been made
* 
* @param: keypad constructor.
* @param: user attempt
*
* @return: SUCCESS or FAILURE
*/
int check_status(Keypad *keypad, char pk_attempt[]);
/**
* resets passkey to "xxxxs"
* 
* @param: passkey
*
* @return: NA
*/
int reset_pk(char passkey[]);

#endif
