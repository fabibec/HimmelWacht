#include <stdio.h>
#include "manual-control.h"
#include "ds4-driver.h"
#include "fire-control.h"
#include "platform-control.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_log.h>

#define MANUAL_CONTROL_TAG "Manual Control"

#define TAP_THRESHOLD_TICKS 1000
#define MAX_SPEED 120.0f
#define TAP_STEP 3.0f
#define HOLD_ACCELERATION 0.05f

static inline void process_fire(uint16_t r2_value);
static inline void process_platform_left_right(int8_t triggerButtons);
static inline void process_platform_up_down(int16_t stickY);

static bool r2_was_pressed = false;
const uint16_t R2_THRESHOLD = 800;

static void manual_control_task(void* arg) {

    static ds4_input_t ds4_current_state;

    // Wait for the DS4 controller to connect
    ds4_wait_for_connection();

    while(1){
        // Wait for an event from the DS4 controller
        xQueueReceive(ds4_input_queue, &ds4_current_state, portMAX_DELAY);

        /*
            TODO:
                - L1 + R1 -> turn platform left/right
                - rY -> turn platform up/down
        */
        process_platform_left_right(ds4_current_state.triggerButtons);
        process_platform_up_down(ds4_current_state.rightStickY);
        process_fire(ds4_current_state.rightTrigger);
    }
}

static inline void process_fire(uint16_t r2_value){

    if (r2_value > R2_THRESHOLD && !r2_was_pressed) {
        fire_control_trigger_shot();
        r2_was_pressed = true;
    } else if (r2_value <= R2_THRESHOLD) {
        r2_was_pressed = false;
    }
}

static inline void process_platform_left_right(int8_t triggerButtons){

    static uint16_t l1_ticks = 0;
    static uint16_t r1_ticks = 0;
    static float l1_speed = 0.0f;
    static float r1_speed = 0.0f;
    static float delta = 0.0f;
    const float dt = 0.016f; // Assuming a 60Hz update rate
    static int8_t platform_x_angle = 0;
    bool update = false;

    // L1 pressed
    if (triggerButtons & 0x01) {
        platform_x_angle += TAP_STEP;
        if(platform_x_angle > 90){
            platform_x_angle = 90;
            ds4_rumble(0, 100, 0xF0, 0xF0);
        } else {
            platform_x_set_angle(platform_x_angle);
        }
    }

    if (triggerButtons & 0x02) {
        platform_x_angle -= TAP_STEP;
        if(platform_x_angle < -90){
            platform_x_angle = -90;
            ds4_rumble(0, 100, 0xF0, 0xF0);
        } else {
            platform_x_set_angle(platform_x_angle);
        }
    }
}

static inline void process_platform_up_down(int16_t stickY){

}

esp_err_t manual_control_init(int8_t core) {
    const char* TAG = "Init";

    // Validate the input
    if(core > 1){
        ESP_LOGE(
            MANUAL_CONTROL_TAG,
            "%s: Invalid core number (%d), must be 0 or 1",
            TAG,
            core
        );
        return ESP_ERR_INVALID_ARG;
    }
    if(ds4_input_queue == NULL){
        ESP_LOGE(
            MANUAL_CONTROL_TAG,
            "%s: DS4 input queue is not initialized",
            TAG
        );
        return ESP_ERR_INVALID_STATE;
    }

    // Create the task for manual control
    BaseType_t task_created = pdFALSE;

    // Create the fire control task
    task_created = xTaskCreatePinnedToCore(
        manual_control_task,   /* Function to implement the task */
        "manualcontrol_task", /* Name of the task */
        4096,       /* Stack size in words */
        NULL,  /* Task input parameter */
        0,          /* Priority of the task */
        NULL,       /* Task handle. */
        core /* Core where the task should run */
    );

    if (task_created != pdTRUE) {
        ESP_LOGE(
            MANUAL_CONTROL_TAG,
            "%s: Failed to create manual control task",
            TAG
        );
        return ESP_FAIL;
    }

    ESP_LOGI(
        MANUAL_CONTROL_TAG,
        "%s: Manual control initialized successfully",
        TAG
    );

    return ESP_OK;
}
