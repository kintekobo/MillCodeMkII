#include "defs.h"
#include "modes.h"
#include "debug.h"
#include "timer.h"
#include "controls.h"


#if USE_LCD
    #if LCD_TYPE_PCF8574
        #include <LiquidCrystal_PCF8574.h>
    #endif  // LCD_TYPE_PCF8574
    #if LCD_TYPE_I2C
        #include <LiquidCrystal_I2C.h>
    #endif // LCD_TYPE_I2C
#endif // USE_LCD

/*
    power-feed - An Arduino-based single-axis milling machine power feed controller.
    Ported from Chris Mower's original millcode project (https://github.com/kintekobo/millcode).

    (C) Damien Walsh <me@damow.net>
*/

// Initialise LCD
#if USE_LCD
    #if LCD_TYPE_PCF8574
        LiquidCrystal_PCF8574 lcd(0x27);
    #endif  // LCD_TYPE_PCF8574
    #if LCD_TYPE_I2C
        // LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 16, 2);
        LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  
    #endif // LCD_TYPE_I2C
#endif // USE_LCD

// Define the appearance of the custom char§ers
uint8_t char_arrow_right[8] = { 0x10, 0x18, 0x1C, 0x1E, 0x1E, 0x1C, 0x18, 0x10 };
uint8_t char_arrow_left[8] = { 0x01, 0x03, 0x07, 0x0F, 0x0F, 0x07, 0x03, 0x01 };
uint8_t char_stop[8] = { 0x00, 0x00, 0x0E, 0x1F, 0x1F, 0x1F, 0x0E, 0x00 };

// The desired precision feedrate (um/sec)
// The "rapid" feedrate is fixed and defined in defs.h
unsigned long desired_feedrate_precision_um_sec = FEEDRATE_PRECISION_DEFAULT_UM_SEC;

// The desired direction (default NONE)
direction_t desired_direction = DIRECTION_NONE;

// The desired mode (default STOP)
mode_t desired_mode = MODE_STOP;

// The reason why the timer has been automatically stopped
extern timer_stop_reason_t timer_stop_reason;

// Has the hardware been configured according to the desired_* parameters above?
bool hardware_configured = false;

/**
 * Set up the system.
 */
void setup()
{
    #ifdef USE_SERIAL
        // Intialise serial debugging
        Serial.begin(115200);
        INFO("Power feed system startup")
    #endif // USE_SERIAL
    
    // Initialise timers
    timer_init();

    // Initialise ports & pins
    pinMode(PIN_ENABLE, OUTPUT);
    pinMode(PIN_DIRECTION, OUTPUT);
    pinMode(PIN_STEP, OUTPUT);
    pinMode(PIN_ENDSTOP, INPUT_PULLUP);
    pinMode(PIN_FAST_LEFT, INPUT_PULLUP);
    pinMode(PIN_FAST_RIGHT, INPUT_PULLUP);
    pinMode(PIN_SLOW_LEFT, INPUT_PULLUP);
    pinMode(PIN_SLOW_RIGHT, INPUT_PULLUP);
    pinMode(PIN_STOP, INPUT_PULLUP);
    pinMode(PIN_ROTARY_A, INPUT_PULLUP);
    pinMode(PIN_ROTARY_B, INPUT_PULLUP);
    pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
    digitalWrite(PIN_ENABLE, HIGH);

    // Intialise the LCD
    #if USE_LCD
        #if LCD_TYPE_PCF8574
            lcd.begin(16, 2);
            lcd.setBacklight(0xff);
        #endif  // LCD_TYPE_PCF8574
        #if LCD_TYPE_I2C
            lcd.begin(16, 2);
	        lcd.backlight();
        #endif // LCD_TYPE_I2C

        // Register some custom characters with the LCD for later use
        lcd.createChar(LCD_CHAR_ARROW_LEFT, char_arrow_left);
        lcd.createChar(LCD_CHAR_ARROW_RIGHT, char_arrow_right);
        lcd.createChar(LCD_CHAR_STOP, char_stop);
    #endif // USE_LCD

    // Attach interrupts for controls
    attach_control_interrupts();
}

/**
 * Check the number of turns made on the rotary encoder.
 */
void check_rotary_turns()
{
    // Was the rotary changed?
    int rotary_turns = get_rotary_turns();
    if (rotary_turns != 0) 
    {
        long adjustment = FEEDRATE_ADJUST_STEP_UM_SEC * rotary_turns;

        // Ensure we don't go below 0 feedrate or above the rapid feedrate
        if ((long)desired_feedrate_precision_um_sec + adjustment <= FEEDRATE_ADJUST_STEP_UM_SEC) 
        {
            desired_feedrate_precision_um_sec = FEEDRATE_ADJUST_STEP_UM_SEC;
        } 
        else if ((long)desired_feedrate_precision_um_sec + adjustment >= FEEDRATE_RAPID_UM_SEC) 
        {
            desired_feedrate_precision_um_sec = FEEDRATE_RAPID_UM_SEC;
        } 
        else 
        {
            desired_feedrate_precision_um_sec = desired_feedrate_precision_um_sec + adjustment;
        }

        hardware_configured = false;
    }
}

/**
 * Check the buttons and update desired mode and desired direction as appropriate.
 */
void check_buttons()
{
    // Were any buttons pushed?
    button_states_t button_states = get_button_states();

    // Rotary button pushed?
    if (button_states & (1 << BUTTON_ROTARY)) 
    {
        // Reset the precision feedrate
        desired_feedrate_precision_um_sec = FEEDRATE_PRECISION_DEFAULT_UM_SEC;
        hardware_configured = false;
    }

    // Fast-left button pushed?
    if (button_states & (1 << BUTTON_FAST_LEFT)) 
    {
        desired_mode = MODE_RAPID;
        desired_direction = DIRECTION_CW;
        hardware_configured = false;
    }

    // Slow-left button pushed?
    if (button_states & (1 << BUTTON_SLOW_LEFT)) 
    {
        desired_mode = MODE_PRECISION;
        desired_direction = DIRECTION_CW;
        hardware_configured = false;
    }

    // Stop button pushed?
    if (button_states & (1 << BUTTON_STOP)) 
    {
        digitalWrite(PIN_ENABLE, HIGH);
        desired_mode = MODE_STOP;
        hardware_configured = false;
    }

    // Slow-right button pushed?
    if (button_states & (1 << BUTTON_SLOW_RIGHT)) 
    {
        desired_mode = MODE_PRECISION;
        desired_direction = DIRECTION_CCW;
        hardware_configured = false;
    }

    // Fast-right button pushed?
    if (button_states & (1 << BUTTON_FAST_RIGHT)) 
    {
        desired_mode = MODE_RAPID;
        desired_direction = DIRECTION_CCW;
        hardware_configured = false;
    }
}

/**
 * Update the LCD display if used
 */
#if USE_LCD

void update_lcd()
{
    // Update the display to reflect the current state
    DEBUG("updating display contents")
    lcd.clear();

    // Write the top line
    lcd.setCursor(0,0);
    lcd.print("Rate: ");

    // Write symbols describing the current state
    lcd.setCursor(13, 0);
    switch (desired_mode) 
    {
        
        case MODE_ENDSTOP:
            lcd.write('E');
            lcd.write('S');
            break;
        
        case MODE_STOP:
            lcd.write(' ');
            lcd.write(LCD_CHAR_STOP);
            break;

        case MODE_RAPID:
            lcd.write(desired_direction == DIRECTION_CW ? LCD_CHAR_ARROW_LEFT : LCD_CHAR_ARROW_RIGHT);
            lcd.write(desired_direction == DIRECTION_CW ? LCD_CHAR_ARROW_LEFT : LCD_CHAR_ARROW_RIGHT);
            break;

        case MODE_PRECISION:
            lcd.write(' ');
            lcd.write(desired_direction == DIRECTION_CW ? LCD_CHAR_ARROW_LEFT : LCD_CHAR_ARROW_RIGHT);
            break;
    }
    lcd.setCursor(0, 1);
    String feedrate_text = String((float)((float)desired_feedrate_precision_um_sec / (float)1000), 3) + " mm/s";
    lcd.print(feedrate_text);
}
#endif // USE_LCD

/**
 * Check if the timer has been automatically stopped (I.e. without our command)
 */
void supervise_timer()
{
    // Check to see if the timer was automatically stopped due to the endstop being reached
    if (timer_stop_reason == REASON_ENDSTOP) 
    {       
        // Reset the timer stop reason
        INFO("timer was stopped automatically due to reaching endstop.")
        timer_stop_reason = REASON_NONE;

        // Change our mode to ENDSTOP
        desired_mode = MODE_ENDSTOP;
        hardware_configured = false;
        return;
    }

    // Check to see if the timer was automatically stopped due to the stop button being pushed
    if (timer_stop_reason == REASON_EMSTOP_PRESSED) 
    {       
        // Reset the timer stop reason
        INFO("timer was stopped automatically due to emstop being observed in interrupt.")
        timer_stop_reason = REASON_NONE;

        // Change our mode to STOP
        desired_mode = MODE_STOP;
        hardware_configured = false;
        return;
    }
}


/**
 * Our main loop inspecs the deisred state (desired_ variables) and configures the timer/output pins if required.
 * All operational halt (stop button, endstop) actions are detected in the timer interrupt itself for safety.
 */
void loop()
{   
    // Supervise the timer to ensure it hasn't been externally stopped
    supervise_timer();

    // Check the rotary encoder turns
    check_rotary_turns();

    // Check button presses
    check_buttons();

    // Does the hardware need to be configured?
    if (!hardware_configured) 
    {

       switch (desired_mode) 
        {
            case MODE_RAPID:
                // Set direction and enable
                digitalWrite(PIN_DIRECTION, desired_direction == DIRECTION_CW ? LOW : HIGH);
                digitalWrite(PIN_ENABLE, LOW);

                // Move at the rapid feedrate
                reconfigure_timer_for_feedrate(FEEDRATE_RAPID_UM_SEC);
                break;

            case MODE_PRECISION:
                // Set direction and enable
                digitalWrite(PIN_DIRECTION, desired_direction == DIRECTION_CW ? LOW : HIGH);
                digitalWrite(PIN_ENABLE, LOW);
                
                // Move at the precision feedrate
                reconfigure_timer_for_feedrate(desired_feedrate_precision_um_sec);
                break;
            
            default:
                // Set the direction and enable pins to their HIGH state
                digitalWrite(PIN_DIRECTION, HIGH);
                digitalWrite(PIN_ENABLE, HIGH);

                // Stop
                timer_stop();
                break;
        }

        // Update our LCD
        #if USE_LCD
            update_lcd();
        #endif /* LCD */

        // We're set up
        DEBUG("peripherals updated")
        hardware_configured = true;
    }
    #ifdef USE_SERIAL
        #ifdef SERIAL_COMMANDS
            handle_serial_commands();
        #endif /* SERIAL_COMMANDS */
    #endif // USE_SERIAL
}
#ifdef USE_SERIAL

/**
 * Service any commands issued on the serial port.
 */
void handle_serial_commands()
{
    // Service commands on the serial port
    if (Serial.available() > 0) 
    {       
        String commandString = Serial.readStringUntil('\r'); 

        // Check for valid commands
        if (commandString.startsWith("stop")) 
        {
            // Stop.
            desired_mode = MODE_STOP;
            hardware_configured = false;
        } 
        else if (commandString.startsWith("fl")) 
        {
            // Change to Rapid Left movement
            desired_mode = MODE_RAPID;
            desired_direction = DIRECTION_CW;
            hardware_configured = false;
        } 
        else if (commandString.startsWith("fr")) 
        {
            // Change to Rapid Right movement
            desired_mode = MODE_RAPID;
            desired_direction = DIRECTION_CCW;
            hardware_configured = false;
        } 
        else if (commandString.startsWith("sl")) 
        {
            // Change to Slow Left movement
            desired_mode = MODE_PRECISION;
            desired_direction = DIRECTION_CW;
            hardware_configured = false;
        } 
        else if (commandString.startsWith("sr")) 
        {
            // Change to Slow Right movement
            desired_mode = MODE_PRECISION;
            desired_direction = DIRECTION_CCW;
            hardware_configured = false;
        } 
        else if (commandString.startsWith("?")) 
        {
            INFO("Possible commands: fl, sl, stop, sr, fr, 1000 (Eg. for 1000um/sec precision feedrate)")
            return;        
        } 
        else 
        {
            // Change the "precision" feedrate
            desired_feedrate_precision_um_sec = commandString.toInt();
            hardware_configured = false;
            INFO_VAL("Set new desired precision feedrate", desired_feedrate_precision_um_sec)
        }
    }
}
#endif // USE_SERIAL
