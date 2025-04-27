/**
 * @file
 * @brief DualShock 4 driver for Esp32
 *
 * This module provides support for the DualShock 4 controller on the Esp32 platform.
 * It utilizes the Bluepad32 library to manage Bluetooth connections and controller interactions.
 *
 * @author Fabian Becker
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_mac.h>
#include <esp_err.h>
#include <esp_log.h>
#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <btstack_stdio_esp32.h>
#include <uni.h>

#include "include/ds4-driver.h"
#include "ds4-common.h"

volatile bool ds4_connected = false;
QueueHandle_t ds4_input_queue = NULL;

static QueueHandle_t ds4_output_event_queue = NULL;

static SemaphoreHandle_t ds4_output_event_callback_sem = NULL;

static btstack_context_callback_registration_t output_event_callback_registration;


void bluepad32_task(void* arg){
    // Defined in ds4-platform.c
    struct uni_platform* get_my_platform(void);

    // Configure BTstack for ESP32 VHCI Controller
    btstack_init();

    // Must be called before uni_init()
    uni_platform_set_custom(get_my_platform());

    // Init Bluepad32.
    uni_init(0, NULL);

    // Does not return.
    btstack_run_loop_execute();
}

static void ds4_rumble_cb(void* context){

    ds4_rumble_t* rumble = (ds4_rumble_t*) context;
    // We only allow 1 connection at a time, so we can assume that the first device is the one we want to use
    uni_hid_device_t* d = uni_hid_device_get_first_device_with_state(UNI_BT_CONN_STATE_DEVICE_READY);

    // Safety checks in case the gamepad got disconnected while the callback was scheduled
    if(d){
        // Take the rumble event from the queue
        if (d->report_parser.play_dual_rumble != NULL)
        d->report_parser.play_dual_rumble(
            d,
            rumble->start_delay_ms,
            rumble->duration_ms,
            rumble->weak_magnitude,
            rumble->strong_magnitude
        );
        ESP_LOGI("DS4 Driver", "Rumble callback: %d %d %d %d",
            rumble->start_delay_ms,
            rumble->duration_ms,
            rumble->weak_magnitude,
            rumble->strong_magnitude
        );
    } else {
        ESP_LOGE("DS4 Driver", "Rumble callback: Device not found");
    }

    // Unlock the output_event_callback_registration for a new callback
    xSemaphoreGiveFromISR(ds4_output_event_callback_sem, NULL);
}

static void ds4_lightbar_cb(void* context){

    ds4_lightbar_color_t lightbar_color = *(ds4_lightbar_color_t*) context;
    // We only allow 1 connection at a time, so we can assume that the first device is the one we want to use
    uni_hid_device_t* d = uni_hid_device_get_first_device_with_state(UNI_BT_CONN_STATE_DEVICE_READY);

    // Safety checks in case the gamepad got disconnected while the callback was scheduled
    if(d){
        if (d->report_parser.set_lightbar_color != NULL)
        d->report_parser.set_lightbar_color(
            d,
            lightbar_color.red,
            lightbar_color.green,
            lightbar_color.blue
        );
    }

    // Unlock the output_event_callback_registration for a new callback
    xSemaphoreGiveFromISR(ds4_output_event_callback_sem, NULL);
}

void ds4_output_event_task(void* arg){
    static ds4_output_event_t event;

    static ds4_rumble_t rumble;
    static ds4_lightbar_color_t lightbar_color;

    // Wait for an event to be available in the queue
    while (xQueueReceive(ds4_output_event_queue, &event, portMAX_DELAY) == pdTRUE) {

        // Call the appropriate function based on the event type
        switch (event.event_type) {
            case DS4_OUTPUT_EVENT_RUMBLE:

                ESP_LOGI("test", "wait for semaphore");
                // Block the output_event_callback_registration (freed after the callback is executed)
                xSemaphoreTake(ds4_output_event_callback_sem, portMAX_DELAY);
                rumble = event.event_params.rumble;
                output_event_callback_registration.callback = &ds4_rumble_cb;
                output_event_callback_registration.context = &rumble;

                btstack_run_loop_execute_on_main_thread(&output_event_callback_registration);
                ESP_LOGI("DS4 Driver", "Rumble event: %d %d %d %d",
                    rumble.start_delay_ms,
                    rumble.duration_ms,
                    rumble.weak_magnitude,
                    rumble.strong_magnitude
                );
                break;
            case DS4_OUTPUT_EVENT_LIGHTBAR_COLOR:

                // Block the output_event_callback_registration (freed after the callback is executed)
                xSemaphoreTake(ds4_output_event_callback_sem, portMAX_DELAY);
                lightbar_color = event.event_params.lightbar;
                output_event_callback_registration.callback = &ds4_lightbar_cb;
                output_event_callback_registration.context = &lightbar_color;

                btstack_run_loop_execute_on_main_thread(&output_event_callback_registration);
                break;
            default:
                break;
        }
    }
}

esp_err_t ds4_init(void){

    // Set the DS4 address
    esp_read_mac(ds4_address, ESP_MAC_BT);
    ESP_LOGI("DS4 Driver Init", "Set DS4 address to %02x:%02x:%02x:%02x:%02x:%02x",
        ds4_address[0], ds4_address[1], ds4_address[2],
        ds4_address[3], ds4_address[4], ds4_address[5]);

    // Create the queue for input events
    ds4_input_queue = xQueueCreate(1, sizeof(ds4_input_t));

    // Create the queue for output events
    ds4_output_event_queue = xQueueCreate(output_event_queue_size, sizeof(ds4_output_event_t));

    // Create the binary semaphore for the output event callback
    ds4_output_event_callback_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(ds4_output_event_callback_sem);

    // Create the bluepad32 task
    xTaskCreatePinnedToCore(
        bluepad32_task,   /* Function to implement the task */
        "bluepad32_task", /* Name of the task */
        8192,       /* Stack size in words */
        NULL,  /* Task input parameter */
        0,          /* Priority of the task */
        NULL,       /* Task handle. */
        0);  /* Core where the task should run */

    // Create the output event task
    xTaskCreatePinnedToCore(
        ds4_output_event_task,   /* Function to implement the task */
        "ds4_output_event_task", /* Name of the task */
        4096,       /* Stack size in words */
        NULL,  /* Task input parameter */
        1,          /* Priority of the task */
        NULL,       /* Task handle. */
        0);  /* Core where the task should run */

    ESP_LOGI("DS4 Driver Init", "DS4 driver initialized");
    return ESP_OK;
}

esp_err_t ds4_rumble(uint16_t start_delay_ms, uint16_t duration_ms, uint8_t weak_magnitude, uint8_t strong_magnitude){

    esp_err_t ret = ESP_OK;

    ds4_rumble_t rumble = {
        .start_delay_ms = start_delay_ms,
        .duration_ms = duration_ms,
        .weak_magnitude = weak_magnitude,
        .strong_magnitude = strong_magnitude
    };
    ds4_output_event_t event = {
        .event_type = DS4_OUTPUT_EVENT_RUMBLE,
        .event_params.rumble = rumble
    };

    // Send the rumble event to the queue, if the queue is full, discard the event
    if (xQueueSendToBack(ds4_output_event_queue, (void *) &event, (TickType_t) 0) != pdTRUE) {
        ESP_LOGE("DS4 Driver", "Failed to send rumble event to queue");
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t ds4_lightbar_color(uint8_t r, uint8_t g, uint8_t b){

    esp_err_t ret = ESP_OK;

    ds4_lightbar_color_t color = {
        .red = r,
        .green = g,
        .blue = b
    };
    ds4_output_event_t event = {
        .event_type = DS4_OUTPUT_EVENT_LIGHTBAR_COLOR,
        .event_params.lightbar = color
    };

    // Send the lightbar event to the queue, if the queue is full, discard the event
    if (xQueueSendToBack(ds4_output_event_queue, (void *) &event, (TickType_t) 0) != pdTRUE) {
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t ds4_get_input(ds4_input_t* input_data){
    // Get the input from the event queue without taking it out of the queue
    if (xQueuePeek(ds4_input_queue, &input_data, 100)) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

bool ds4_is_connected(void){
    return ds4_connected;
}
