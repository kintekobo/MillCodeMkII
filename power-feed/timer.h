#ifndef __TIMER_H__
#define __TIMER_H__

#include "Arduino.h"

// The number of T2 prescaler configurations defined in timer.c
#define T2_PRESCALER_NUM 5

/**
 * A configuration of T2's prescaler.
 */
typedef struct
{
    // The TC2 Control Register B state for this configuration
    byte tccr2b;

    // The clock denominator
    unsigned int denominator;

    // The step resolution in microseconds for this prescaler
    unsigned long increment;

} t2_prescaler_config_t;

/**
 * Reasons why the timer may halt automatically.
 */
typedef enum 
{
    REASON_NONE,                // Not stopped
    REASON_ENDSTOP,             // Endstop reached
    REASON_EMSTOP_PRESSED       // Emergency stop pressed
} timer_stop_reason_t;

// Methods
void timer_init();
void timer_stop();
bool reconfigure_timer_for_feedrate(unsigned long rate_us_sec);

#endif