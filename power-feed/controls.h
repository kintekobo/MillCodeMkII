#ifndef __CONTROLS_H__
#define __CONTROLS_H__

#include "Arduino.h"

#define BUTTON_FAST_LEFT 1
#define BUTTON_SLOW_LEFT 2
#define BUTTON_SLOW_RIGHT 3
#define BUTTON_FAST_RIGHT 4
#define BUTTON_STOP 5
#define BUTTON_ROTARY 6

/**
 * A type to store the state of the buttons.
 */
typedef uint8_t button_states_t;

void attach_control_interrupts();
int get_rotary_turns();
button_states_t get_button_states();

#endif