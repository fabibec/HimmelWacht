#ifndef _DS4_CONSTS_H_
#define _DS4_CONSTS_H_
#include <stdint.h>

/**/
typedef uint8_t bd_addr_t[6];
extern const bd_addr_t ds4_address;

/* Masks to access the Dualshock4 specific buttons from the general mapping */
#define DPAD_UP_MASK 0x01
#define DPAD_DOWN_MASK 0x02
#define DPAD_RIGHT_MASK 0x04
#define DPAD_LEFT_MASK 0x08

#define BUTTON_CROSS_MASK 0x0001
#define BUTTON_CIRCLE_MASK 0x0002
#define BUTTON_SQUARE_MASK 0x0004
#define BUTTON_TRIANGLE_MASK 0x0008

#define BUTTON_L1_MASK 0x0010
#define BUTTON_R1_MASK 0x0020

/* Input processing rate */
extern const uint8_t input_processing_freq_hz;
extern const uint16_t input_processing_interval_us;

typedef struct {
    int16_t leftTrigger, rightTrigger; // -512 to 512
    int16_t leftStickX, leftStickY; // -512 to 512
    int16_t rightStickX, rightStickY; // -512 to 512
    uint8_t dpad; // 0x01 = up, 0x02 = down, 0x04 = right, 0x08 = left
    uint8_t buttons; // 0x01 = cross, 0x02 = circle, 0x04 = square, 0x08 = triangle
    uint8_t triggerButtons; // 0x01 = L1, 0x02 = R1
} ds4_input_t;

typedef struct {
    uint16_t start_delay_ms, duration_ms;
    uint8_t weak_magnitude, strong_magnitude;
} ds4_rumble_context_t;

typedef struct {
    uint8_t red, green, blue;
} ds4_lightbar_color_t;

#endif // _DS4_CONSTS_H_
