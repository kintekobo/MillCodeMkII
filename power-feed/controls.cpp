#include "Arduino.h"
#include "controls.h"
#include "defs.h"
#include "debug.h"

// Keep track of the number of rotary turns since last check.
static volatile int rotary_turns = 0;

// Keep track of button pushes
static volatile button_states_t button_states = 0;

/**
 * ISR for buttons being pressed.
 */
ISR(PCINT0_vect)
{
    // Primitive debouncing
    delayMicroseconds(1000);

    // Capture the state of all buttons
    button_states |= (digitalRead(PIN_FAST_LEFT) == LOW) << BUTTON_FAST_LEFT;
    button_states |= (digitalRead(PIN_SLOW_LEFT) == LOW) << BUTTON_SLOW_LEFT;
    button_states |= (digitalRead(PIN_SLOW_RIGHT) == LOW) << BUTTON_SLOW_RIGHT;
    button_states |= (digitalRead(PIN_FAST_RIGHT) == LOW) << BUTTON_FAST_RIGHT;
    button_states |= (digitalRead(PIN_ROTARY_SW) == LOW) << BUTTON_ROTARY;
    button_states |= (digitalRead(PIN_STOP) == LOW) << BUTTON_STOP;
}

/**
 * ISR for the rotary encoder being turned.
 */
ISR(PCINT2_vect)
{
    if (digitalRead(PIN_ROTARY_B) == LOW) {
        // Increment or decrement rotary_turns based on which direction the rotary was turned
        rotary_turns += (digitalRead(PIN_ROTARY_A) == HIGH ? 1 : -1) * ENCODER_DIRECTION;
    }
}

/**
 * Initialise control interrupts.
 */
void attach_control_interrupts()
{
    DEBUG("attaching ISRs for controls")

    // Pin Change Interrupts enabled for PCIE0 and PCIE2
    PCICR |= bit(PCIE0) | bit(PCIE2);

    // 5 buttons + rotary on PCIE0
    PCMSK0 |= 0b00111111;

    // Just the single rotary quadrature on PCIE2
    PCMSK2 |= 0b10000000;
}

/**
 * This subroutine returns the state of the control buttons.
 */
button_states_t get_button_states()
{
    button_states_t button_states_atomic;

    cli();
    button_states_atomic = button_states;
    button_states = 0;
    sei();

    return button_states_atomic;
}

/**
 * Get & reset the number of rotary turns since the last check.
 * 
 * We could be preempted during this function, so interrupts are disabled for the critical section
 * of reading and writing (resetting) the number of turns.
 */
int get_rotary_turns()
{
    int rotary_turns_atomic;

    cli();
    rotary_turns_atomic = rotary_turns;
    rotary_turns = 0;
    sei();

    return rotary_turns_atomic;
}