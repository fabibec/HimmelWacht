/**
 * @file
 * @brief Shared constants and datatypes for the DualShock 4 driver
 *
 * @author Fabian Becker
 */
#ifndef _DS4_CONSTS_H_
#define _DS4_CONSTS_H_
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <stdbool.h>
#include "ds4-driver.h"

typedef uint8_t bd_addr_t[6];
// Address of the DS4 Controller
extern bd_addr_t ds4_address;

// Event group for connection & low battery events
extern EventGroupHandle_t ds4_event_group;

// Event bit for DS4 controller connection
#define DS4_CONNECTED (1 << 0)

// Event bit for DS4 controller battery level
// This is used to indicate that the battery level is below the threshold
// This locks the lightbar
#define DS4_BATTERY_LOW (1 << 1)
extern const uint8_t low_battery_threshold;
extern const uint16_t low_battery_blinking_interval_ms;

// Queue for the controller input
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

// Input processing rate (Events per second)
extern const uint8_t input_processing_freq_hz;

// Input processing interval (for timer control, based on input_processing_freq_hz)
extern const uint32_t input_processing_interval_us;

// Queue size of the output event queue
extern const uint8_t output_event_queue_size;

// Rumble event
typedef struct {
    uint16_t start_delay_ms, duration_ms;
    uint8_t weak_magnitude, strong_magnitude;
} ds4_rumble_t;

// Lightbar Event
typedef struct {
    uint8_t red, green, blue;
} ds4_lightbar_color_t;

// Event union
typedef union {
    ds4_rumble_t rumble;
    ds4_lightbar_color_t lightbar;
} ds4_output_event_params_t;

// Event type enum
typedef enum {
    DS4_OUTPUT_EVENT_RUMBLE = 0,
    DS4_OUTPUT_EVENT_LIGHTBAR_COLOR = 1,
} ds4_output_event_type_t;

// Shared type for the output event queue
typedef struct {
    ds4_output_event_type_t event_type;
    ds4_output_event_params_t event_params;
} ds4_output_event_t;

#endif // _DS4_CONSTS_H_
