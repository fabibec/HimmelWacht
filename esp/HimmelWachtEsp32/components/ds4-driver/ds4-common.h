#ifndef _DS4_CONSTS_H_
#define _DS4_CONSTS_H_
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdbool.h>
#include "ds4-driver.h"

//Address of the DS4 Controller
typedef uint8_t bd_addr_t[6];
extern bd_addr_t ds4_address;

// Event bit for DS4 controller connection
#define DS4_CONNECTED (1 << 0)
extern EventGroupHandle_t ds4_event_group;

// Event bit for DS4 controller battery level
// This is used to indicate that the battery level < 20%
// This looks the lightbar for custom color
#define DS4_BATTERY_LOW (1 << 1)
extern const uint8_t low_battery_blinking_freq_hz;
extern const uint32_t low_battery_blinking_interval_us;

extern QueueHandle_t ds4_input_queue;


// Masks to access the Dualshock4 specific buttons from the general bluepad32 mapping
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
extern const uint32_t input_processing_interval_us;

extern const uint8_t output_event_queue_size;

typedef struct {
    uint16_t start_delay_ms, duration_ms;
    uint8_t weak_magnitude, strong_magnitude;
} ds4_rumble_t;

typedef struct {
    uint8_t red, green, blue;
} ds4_lightbar_color_t;

typedef union {
    ds4_rumble_t rumble;
    ds4_lightbar_color_t lightbar;
} ds4_output_event_params_t;

typedef enum {
    DS4_OUTPUT_EVENT_RUMBLE = 0,
    DS4_OUTPUT_EVENT_LIGHTBAR_COLOR = 1,
} ds4_output_event_type_t;

typedef struct {
    ds4_output_event_type_t event_type;
    ds4_output_event_params_t event_params;
} ds4_output_event_t;

#endif // _DS4_CONSTS_H_
