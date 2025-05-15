#include <stdio.h>
#include "manual-control.h"
#include "ds4-driver.h"
#include "fire-control.h"
#include "platform-control.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <math.h>
#include <esp_log.h>

#define MANUAL_CONTROL_TAG "Manual Control"

#define TAP_THRESHOLD_TICKS 1000
#define MAX_SPEED 150.0f
#define TAP_STEP 3.0f
#define HOLD_ACCELERATION 0.05f

#define L1 0x01
#define R1 0x02

#define MAX_DEG_PER_SECOND 300.0f
#define DT 0.016f // Assuming a 60Hz update rate
#define ALPHA 0.f

static inline void process_fire(uint16_t r2_value);
static inline void process_platform_left_right(int8_t triggerButtons);
static inline void process_platform_up_down(int16_t stickY);


static int8_t platform_x_angle = 0;
static int8_t platform_y_angle = 0;
static int8_t deadzone = 30;

static void manual_control_task(void* arg) {

    static ds4_input_t ds4_current_state;

    // Wait for the DS4 controller to connect
    ds4_wait_for_connection();

    while(1){
        // Wait for an event from the DS4 controller
        xQueueReceive(ds4_input_queue, &ds4_current_state, portMAX_DELAY);

        process_platform_left_right(ds4_current_state.triggerButtons);
        process_platform_up_down(ds4_current_state.rightStickY);
        process_fire(ds4_current_state.rightTrigger);
    }
}

static inline void process_fire(uint16_t r2_value){
    static bool r2_was_pressed = false;
    const uint16_t R2_THRESHOLD = 800;

    if (r2_value > R2_THRESHOLD && !r2_was_pressed) {
        fire_control_trigger_shot();
        r2_was_pressed = true;
    } else if (r2_value <= R2_THRESHOLD) {
        r2_was_pressed = false;
    }
}

static inline void process_platform_left_right(int8_t triggerButtons){

    if (triggerButtons & L1) {
        platform_x_angle += TAP_STEP;
        if(platform_x_angle > platform_get_x_right_stop_angle()){
            platform_x_angle = platform_get_x_right_stop_angle();
            ds4_rumble(0, 100, 0xF0, 0xF0);
        } else {
            platform_x_set_angle(platform_x_angle);
        }
    }

    if (triggerButtons & R1) {
        platform_x_angle -= TAP_STEP;
        if(platform_x_angle < platform_get_x_left_stop_angle()){
            platform_x_angle = platform_get_x_left_stop_angle();
            ds4_rumble(0, 100, 0xF0, 0xF0);
        } else {
            platform_x_set_angle(platform_x_angle);
        }
    }
}

static inline void process_platform_up_down(int16_t stickY){
    if(abs(stickY) < deadzone) {
        stickY = 0;
    }

    float normalized_stickY = (float)stickY / 512.0f;
    //normalized_stickY = ALPHA * normalized_stickY + (1 - ALPHA) * normalized_stickY;

    float speed = normalized_stickY * MAX_SPEED;
    platform_y_angle -= speed * DT;

    if(platform_y_angle < platform_get_y_left_stop_angle()){
        platform_y_angle = platform_get_y_left_stop_angle();
        ds4_rumble(0, 100, 0xF0, 0xF0);
    } else if (platform_y_angle > platform_get_y_right_stop_angle()){
        platform_y_angle = platform_get_y_right_stop_angle();
        ds4_rumble(0, 100, 0xF0, 0xF0);
    }

    platform_y_set_angle(platform_y_angle);
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

    // Set the initial angles
    platform_x_angle = platform_get_x_start_angle();
    platform_y_angle = platform_get_y_start_angle();

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
