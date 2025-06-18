#include <stdio.h>
#include "vehicle-control.h"
#include "ds4-driver.h"
#include "ds4-common.h"
#include "fire-control.h"
#include "platform-control.h"
#include "mqtt-stack.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <math.h>
#include <stdbool.h>
#include <IQmathLib.h>
#include <esp_log.h>
#include <esp_timer.h>

#define VEHICLE_CONTROL_TAG "Vehicle Control"

typedef enum{
    MANUAL_TURRET_CONTROL,
    AUTOMATIC_TURRET_CONTROL
} vehicle_state_t;
static vehicle_state_t vehicle_state = MANUAL_TURRET_CONTROL;

#define MANUAL_MODE_COLOR_R 80
#define MANUAL_MODE_COLOR_G 200
#define MANUAL_MODE_COLOR_B 120

#define AUTO_MODE_COLOR_R 255
#define AUTO_MODE_COLOR_G 180
#define AUTO_MODE_COLOR_B 80


static inline void process_fire(uint16_t r2_value);
static inline void process_manual_platform_left_right(int16_t stickX);
static inline void process_manual_platform_up_down(int16_t stickY);
static inline void process_platform_left_right();
static inline void process_platform_up_down();
static inline void process_drive(diff_drive_handle_t *diff_drive, int16_t x, int16_t y);

static int8_t platform_x_angle = 0;
static int8_t platform_y_angle = 0;
static int8_t deadzone_x = 0;
static int8_t deadzone_y = 0;

static int16_t diff_drive_prev_x = 0;
static int16_t diff_drive_prev_y = 0;

static _iq21 max_deg_per_sec_x = 0;
static _iq21 max_deg_per_sec_y = 0;

static _iq21 dt = 0;

static int64_t button_hold_threshold_us = 0;

typedef struct {
    bool is_held;
    int64_t press_time;
    void (*action)(void);
    bool action_triggered;
} ButtonHoldState;

static ButtonHoldState platform_angle_reset_button_state = {0};
static ButtonHoldState vehicle_mode_change_button_state = {0};

/*
    Reset the platform angles to the starting position

    @author Fabian Becker
*/
static inline void reset_platform_angles(void) {
    platform_x_to_start(&platform_x_angle);
    platform_y_to_start(&platform_y_angle);
    ds4_rumble(0, 100, 0xF0, 0xF0);
}

static inline void set_vehicle_mode_color(void) {
    if(vehicle_state == MANUAL_TURRET_CONTROL){
        ds4_lightbar_color(MANUAL_MODE_COLOR_R, MANUAL_MODE_COLOR_G, MANUAL_MODE_COLOR_B);
    } else {
        ds4_lightbar_color(AUTO_MODE_COLOR_R, AUTO_MODE_COLOR_G, AUTO_MODE_COLOR_B);
    }
}

/*
    Change the vehicle mode between manual and automatic turret control

    @author Fabian Becker
*/
static inline void change_vehicle_mode(void) {
    if(vehicle_state == MANUAL_TURRET_CONTROL){
        vehicle_state = AUTOMATIC_TURRET_CONTROL;
        //ds4_lightbar_color(MANUAL_MODE_COLOR_R, MANUAL_MODE_COLOR_G, MANUAL_MODE_COLOR_B);
        platform_reset(&platform_x_angle, &platform_y_angle);
    } else {
        //ds4_lightbar_color(MANUAL_MODE_COLOR_R, MANUAL_MODE_COLOR_G, MANUAL_MODE_COLOR_B);
        vehicle_state = MANUAL_TURRET_CONTROL;
    }
    set_vehicle_mode_color();
    ds4_rumble(0, 100, 0xF0, 0xF0);
}

static bool check_button_hold(bool is_pressed, ButtonHoldState *button){
    if (is_pressed) {
        if (!button->is_held) {
            // Button just pressed, record the press time
            button->press_time = esp_timer_get_time();
            button->is_held = true;
        } else {
            // Check if the button has been held long enough and action has not been triggered yet
            int64_t held_time = esp_timer_get_time() - button->press_time;
            if (held_time >= button_hold_threshold_us && !button->action_triggered) {
                if (button->action) {
                    button->action();
                }
                button->action_triggered = true;
                return true;
            }
        }
    } else {
        // Button released, reset state
        button->is_held = false;
        button->action_triggered = false;
        button->press_time = 0;
    }
    return false;
}

static void vehicle_control_task(void* arg) {
    diff_drive_handle_t *diff_drive = (diff_drive_handle_t *)arg;
    static ds4_input_t ds4_current_state;
    static uint16_t platform_x_input = 0;
    static uint16_t platform_y_input = 0;
    static uint16_t platform_fire_input = 0;

    while(1){
        // Wait for the DS4 controller to connect
        ds4_wait_for_connection();

        // Wait for an event from the DS4 controller
        xQueueReceive(ds4_input_queue, &ds4_current_state, portMAX_DELAY);

        // Drive can be controlled manually all the time
        process_drive(diff_drive, ds4_current_state.leftStickX, ds4_current_state.leftStickY * (-1));

        // Check for mode change via button hold
        bool buttons_pressed = (ds4_current_state.dpad & DPAD_UP_MASK) && (ds4_current_state.buttons & BUTTON_CROSS_MASK);
        if(check_button_hold(buttons_pressed, &vehicle_mode_change_button_state)) continue;

        // Turret can be controlled manually or automatically
        if(vehicle_state == MANUAL_TURRET_CONTROL){
            // Check for reset via button hold
            if(check_button_hold(ds4_current_state.buttons & BUTTON_CIRCLE_MASK, &platform_angle_reset_button_state)) continue;

            platform_x_input = ds4_current_state.rightStickX;
            platform_y_input = ds4_current_state.rightStickY;

            process_manual_platform_left_right(platform_x_input);
            process_manual_platform_up_down(platform_y_input);
        } else if(vehicle_state == AUTOMATIC_TURRET_CONTROL){
            mqtt_turret_cmd_t mqtt_cmd;
            
            if(mqtt_stack_get_turret_command(&mqtt_cmd) == ESP_OK) {
                // Update platform positions
                platform_x_angle = mqtt_cmd.platform_x_angle;
                platform_y_angle = mqtt_cmd.platform_y_angle;

                process_platform_left_right();
                process_platform_up_down();
                
                // Enhancement for full automatic control (already implemented in the MQTT stack and works):
                // if(mqtt_cmd.fire_command) {
                //     fire_control_trigger_shot();
                // }
            } else {
                // stay in position
            }
        }

        platform_fire_input = ds4_current_state.rightTrigger;
        process_fire(platform_fire_input);

        set_vehicle_mode_color();
    }
}

static inline void process_drive(diff_drive_handle_t *diff_drive, int16_t x, int16_t y){
    ESP_LOGI(VEHICLE_CONTROL_TAG, "x: %d, y: %d", x, y);
    //check if new x, y is bigger than the deadzone compared to the previous x, y
    if(abs(x - diff_drive_prev_x) < 75 && abs(y - diff_drive_prev_y) < 75){
        return;
    }

    // Save the previous x, y values
    diff_drive_prev_x = x;
    diff_drive_prev_y = y;

    input_matrix_t matrix = {
        .x = x,
        .y = y};

    esp_err_t ret = diff_drive_send_cmd(diff_drive, &matrix);
    if (ret != ESP_OK)
    {
        ESP_LOGE(VEHICLE_CONTROL_TAG, "Failed to send command: %s", esp_err_to_name(ret));
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

static inline void process_manual_platform_left_right(int16_t stickX){
    // Older controllers have stick drift and therefore small deviations from 0 need to be ignored
    if(abs(stickX) < deadzone_x) {
        stickX = 0;
    }

    /*
        The stick value is used as a speed value, constraint by the servos max speed.
        (The servos max speed is > 150 degrees per second, but turning the platform to fast decreases the accuracy)
        The speed is then used to calculate the new angle of the platform.
    */
    _iq21 normalized_stickX = _IQ21div(_IQ21(stickX), _IQ21(512));
    _iq21 speed = _IQ21mpy(normalized_stickX, max_deg_per_sec_x);
    platform_x_angle -= _IQ21mpy(speed, dt) >> 21;

    process_platform_left_right();

}

static inline void process_platform_left_right(){
    int8_t set_angle = 0;
    platform_x_set_angle(platform_x_angle, &set_angle);

    // Give return feedback if the angle was clipped
    if(set_angle != platform_x_angle){
        platform_x_angle = set_angle;
        ds4_rumble(0, 100, 0xF0, 0xF0);
    }
}

static inline void process_manual_platform_up_down(int16_t stickY){
    // Older controllers have stick drift and therefore small deviations from 0 need to be ignored
    if(abs(stickY) < deadzone_y) {
        stickY = 0;
    }

    /*
        The stick value is used as a speed value, constraint by the servos max speed.
        (The servos max speed is > 150 degrees per second, but turning the platform to fast decreases the accuracy)
        The speed is then used to calculate the new angle of the platform.
    */
    _iq21 normalized_stickY = _IQ21div(_IQ21(stickY), _IQ21(512));
    _iq21 speed = _IQ21mpy(normalized_stickY, max_deg_per_sec_y);
    platform_y_angle -= _IQ21mpy(speed, dt) >> 21;

    process_platform_up_down();
}

static inline void process_platform_up_down(){
    int8_t set_angle = 0;
    platform_y_set_angle(platform_y_angle, &set_angle);

    // Give return feedback if the angle was clipped
    if(set_angle != platform_y_angle){
        platform_y_angle = set_angle;
        ds4_rumble(0, 100, 0xF0, 0xF0);
    }
}

esp_err_t vehicle_control_init(manual_control_config_t* cfg, diff_drive_handle_t *diff_drive) {
    const char* TAG = "Init";

    // Validate the input
    if(cfg == NULL){
        ESP_LOGE(
            VEHICLE_CONTROL_TAG,
            "%s: Invalid configuration, cfg is NULL",
            TAG
        );
        return ESP_ERR_INVALID_ARG;
    }
    if(cfg->core > 1){
        ESP_LOGE(
            VEHICLE_CONTROL_TAG,
            "%s: Invalid core number (%d), must be 0 or 1",
            TAG,
            cfg->core
        );
        return ESP_ERR_INVALID_ARG;
    }

    // Assign the configuration values
    button_hold_threshold_us = cfg->button_hold_threshold_us;
    deadzone_y = cfg->deadzone_y;
    deadzone_x = cfg->deadzone_x;
    max_deg_per_sec_x = _IQ21(cfg->max_deg_per_sec_x);
    max_deg_per_sec_y = _IQ21(cfg->max_deg_per_sec_y);
    dt = _IQ21div(_IQ21(1), _IQ21(cfg->input_processing_freq_hz));

    platform_angle_reset_button_state.action = reset_platform_angles;
    vehicle_mode_change_button_state.action = change_vehicle_mode;

    if(ds4_input_queue == NULL){
        ESP_LOGE(
            VEHICLE_CONTROL_TAG,
            "%s: DS4 input queue is not initialized",
            TAG
        );
        return ESP_ERR_INVALID_STATE;
    }

    // Set the initial angles, this is already done in the platform_init function, but this is used to get the current angle
    platform_reset(&platform_x_angle, &platform_y_angle);

    // Create the task for manual control
    BaseType_t task_created = pdFALSE;

    // Create the fire control task
    task_created = xTaskCreatePinnedToCore(
        vehicle_control_task,   /* Function to implement the task */
        "vehicle_control_task", /* Name of the task */
        4096,       /* Stack size in words */
        diff_drive,  /* Task input parameter */
        0,          /* Priority of the task */
        NULL,       /* Task handle. */
        cfg->core /* Core where the task should run */
    );

    if (task_created != pdTRUE) {
        ESP_LOGE(
            VEHICLE_CONTROL_TAG,
            "%s: Failed to create vehicle control task",
            TAG
        );
        return ESP_FAIL;
    }

    ESP_LOGI(
        VEHICLE_CONTROL_TAG,
        "%s: Vehicle control initialized successfully",
        TAG
    );

    return ESP_OK;
}