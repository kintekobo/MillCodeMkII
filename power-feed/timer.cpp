#include "timer.h"
#include "debug.h"
#include "Arduino.h"
#include "defs.h"
#include "modes.h"

/**
 * Define the available T2 prescaler configurations, ordered fastest rate to slowest rate.
 */
const t2_prescaler_config_t t2_prescalers[T2_PRESCALER_NUM] = {
    { .tccr2b = 0b011, .denominator = 32  , .increment = 1000000 / (F_CPU / 32  ) },
    { .tccr2b = 0b100, .denominator = 64  , .increment = 1000000 / (F_CPU / 64  ) },
    { .tccr2b = 0b101, .denominator = 128 , .increment = 1000000 / (F_CPU / 128 ) },
    { .tccr2b = 0b110, .denominator = 256 , .increment = 1000000 / (F_CPU / 256 ) },
    { .tccr2b = 0b111, .denominator = 1024, .increment = 1000000 / (F_CPU / 1024) }
};

// The reason for the timer being stopped automatically.
volatile timer_stop_reason_t timer_stop_reason = REASON_NONE;

// The direction we were moving when the timer was automatically stopped.
volatile direction_t direction_on_timer_stop = DIRECTION_NONE;

// Count endstop-triggered pulses (debounce)
volatile int endstopped_pulses = 0;

// Our current direction
extern direction_t desired_direction;

/**
 * Interrupt handler for the T2 Comparison match
 */
SIGNAL(TIMER2_COMPA_vect) 
{
    if (digitalRead(PIN_ENDSTOP) == HIGH) {
        endstopped_pulses ++;
    } else {
        endstopped_pulses = 0;
    }
  
    // Check the endstop hasn't been reached, if it has, stop the timer and set the stop reason flag
    if (endstopped_pulses >= ENDSTOP_EXPOSURE_TICKS) {

        endstopped_pulses = 0;

        // If we don't currently have a direction stored, remember the current one
        if (direction_on_timer_stop == DIRECTION_NONE) {
            direction_on_timer_stop = desired_direction;
        }

        // If we're moving the same way, halt
        if (!ALLOW_BACK_AWAY_FROM_ENDSTOP || desired_direction == direction_on_timer_stop) {

            // Stop the timer
            timer_stop();

            // Set the timer stop reason - reached endstop
            timer_stop_reason = REASON_ENDSTOP;
            
            return;
        }

    } else {

        // Clear the timer stop direction
        direction_on_timer_stop = DIRECTION_NONE;

    }

    // The emstop button will also prevent pulses from being output
    if (digitalRead(PIN_STOP) == LOW) {
        
        // Set the timer stop reason - emstop
        timer_stop_reason = REASON_EMSTOP_PRESSED;

        // Stop the timer
        timer_stop();

        return;
    }

    PORTD |= 0b00001000;
    delayMicroseconds(STEP_PULSE_WIDTH_US);
    PORTD &= ~0b00001000;
}

/**
 * Initialise timers.
 * Configure's the AVR's TC2 8-bit timer to generate interrupts at a 
 */
void timer_init() 
{
    cli();

    // Start timer 2 at value 0
    TCNT2 = 0;

    // Set the Waveform Generation Mode
    // Timer will run in CTC (Clear Timer on Comparison) mode
    TCCR2A = (1 << WGM21);

    // Timer is disabled initially
    TCCR2B = 0;

    // Clear and retrigger when timer reaches this value
    OCR2A = 0;

    // Interrupt on timer compare match
    TIMSK2 |= (1 << OCIE2A);

    // Disable other timers that could introduce jitter into our generated signal
    TIMSK0 &= ~(1 << TOIE0);

    sei();
}

/**
 * Stop the T2 timer immediately.
 */
void timer_stop()
{
    cli();
    // Set T2 to have no clock source (Timer/Counter stopped)
    TCCR2B = 0;
    sei();
}

/**
 * Set T2 timer up for a new feedrate in micrometers/second.
 * 
 * Returns true when the feedrate is successfully changed.
 * Returns false if the feedrate cannot be set due to hardware limitations.
 */
bool reconfigure_timer_for_feedrate(unsigned long rate_um_sec)
{
    unsigned long delay_us = 1000000000L / (rate_um_sec * STEPS_PER_MM) - STEP_PULSE_WIDTH_US;

    // If we're just being asked to set to 0, just stop the timer
    if (rate_um_sec == 0) {
        INFO("zero feedrate requested: stopping")
        timer_stop();
        return true;
    }

    DEBUG_VAL("reconfigure_timer_for_feedrate: new desired feedrate delay", delay_us)

    // If the delay is smaller than the fastest prescaler's minimum increment, we can't proceed
    if (delay_us < t2_prescalers[0].increment) {
        // Stop the timer immediately if an invalid configuration is requested
        ERROR_VAL("No prescaler fast enough for the desired feedrate delay (uS)", delay_us)
        timer_stop();
        return false;
    }

    // Find an appropriate prescaler configuration
    for (unsigned int ps_idx = 0; ps_idx < T2_PRESCALER_NUM; ps_idx ++) {

        // Check if the maxium period for this prescaler will accomodate the desired delay
        if (delay_us <= t2_prescalers[ps_idx].increment * 255) {

            INFO_VAL("selected new T2 prescaler", t2_prescalers[ps_idx].denominator)
            
            // Calculate the number of increments for this prescaler
            unsigned int timer_ticks = delay_us / t2_prescalers[ps_idx].increment; 
            INFO_VAL("selected new T2 compare value", timer_ticks)
            long actual_delay = (long)timer_ticks * (long)t2_prescalers[ps_idx].increment;
            INFO_VAL("actual feedrate delay (uS) will be", actual_delay)
            INFO_VAL("feedrate delay variance (uS) will be", actual_delay - (long)delay_us)
            
            // Disable interrupts and reconfigure T2's prescaler and compare value
            cli();
            TCCR2B = t2_prescalers[ps_idx].tccr2b;
            OCR2A = timer_ticks & 0xff;
            sei();
            return true;
        }
    }

    // Stop the timer immediately if an invalid configuration is requested
    ERROR_VAL("No prescaler slow enough for the desired feedrate delay (uS)", delay_us)
    timer_stop();

    return false;
}
