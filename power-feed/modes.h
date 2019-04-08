#ifndef __MODES_H__
#define __MODES_H__

/**
 * Define the possible movement directions.
 */
typedef enum {
    DIRECTION_CW,
    DIRECTION_CCW,
    DIRECTION_NONE
} direction_t;

/**
 * Define the possible movement modes.
 */
typedef enum {
    MODE_PRECISION,
    MODE_RAPID,
    MODE_STOP,
    MODE_ENDSTOP
} mode_t;

#endif