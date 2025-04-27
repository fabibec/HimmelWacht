#ifndef DS4_DRIVER_H
#define DS4_DRIVER_H
#include <stdint.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdbool.h>

// Attach to this queue to receive input events
extern QueueHandle_t ds4_input_queue;

typedef struct {
    int16_t leftTrigger, rightTrigger; // -512 to 512
    int16_t leftStickX, leftStickY; // -512 to 512
    int16_t rightStickX, rightStickY; // -512 to 512
    uint8_t dpad; // 0x01 = up, 0x02 = down, 0x04 = right, 0x08 = left
    uint8_t buttons; // 0x01 = cross, 0x02 = circle, 0x04 = square, 0x08 = triangle
    uint8_t triggerButtons; // 0x01 = L1, 0x02 = R1
} ds4_input_t;

esp_err_t ds4_init(void);

esp_err_t ds4_rumble(uint16_t start_delay_ms, uint16_t duration_ms, uint8_t weak_magnitude, uint8_t strong_magnitude);
esp_err_t ds4_lightbar_color(uint8_t red, uint8_t green, uint8_t blue);

bool ds4_is_connected(void);
esp_err_t ds4_get_input(ds4_input_t *input_data);

#endif // DS4_DRIVER_H
