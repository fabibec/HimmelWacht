#include "sdkconfig.h"

#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "ds4-driver.h"
#include "manual-control.h"
#include "motor-driver.h"
#include "diff-drive.h"

#define PWM_CHANNEL 0

// Define GPIO pins for motors
#define RIGHT_MOTOR_PWM_GPIO 23
#define RIGHT_MOTOR_DIR_GPIO 22
#define RIGHT_MOTOR_FAULT_GPIO 21
#define RIGHT_MOTOR_FAULT_LED 3

#define LEFT_MOTOR_PWM_GPIO 27
#define LEFT_MOTOR_DIR_GPIO 26
#define LEFT_MOTOR_FAULT_GPIO 25
#define LEFT_MOTOR_FAULT_LED 32


#define MAX_INPUT_VALUE 512


void app_main(void) {
    manual_control_config_t manual_control_cfg = {
        .button_hold_threshold_us = 1500000, // 1.5 seconds
        .max_deg_per_sec_x = 300,
        .max_deg_per_sec_y = 100,
        .input_processing_freq_hz = 60,
        .deadzone_x = 75,
        .deadzone_y = 75,
        .deadzone_drive_update = 10,
        .core = 1
    };

    // Differential drive configuration
    diff_drive_config_t diff_drive_config = {
        .max_input = MAX_INPUT_VALUE,
        .cmd_queue_size = 10,
        .recovery_time_ms = 1000,
        .task_priority = 0,
        .task_stack_size = 4096,
        .task_core_id = 0,
        .task_delay_ms = 80,
        .queue_timout_ms = 10,
    };

    // Left motor configuration
    motor_config_t left_motor_config = {
        .mcpwm_unit = MCPWM_UNIT_0,
        .timer_num = MCPWM_TIMER_0,
        .generator = MCPWM_OPR_A,
        .pwm_signal = MCPWM0A,
        .pwm_gpio_num = LEFT_MOTOR_PWM_GPIO,
        .dir_gpio_num = LEFT_MOTOR_DIR_GPIO,
        .fault_gpio_num = LEFT_MOTOR_FAULT_GPIO,
        .fault_led_gpio_num = LEFT_MOTOR_FAULT_LED,
        .pwm_frequency_hz = 20000,
        .ramp_rate = 5,            // Adjust as needed
        .ramp_intervall_ms = 10,   // Adjust as needed
        .direction_hysteresis = 5, // Adjust as needed
        .pwm_duty_limit = 100,
        .mynr = 0};

    // Right motor configuration
    motor_config_t right_motor_config = {
        .mcpwm_unit = MCPWM_UNIT_0,
        .timer_num = MCPWM_TIMER_1,
        .generator = MCPWM_OPR_A,
        .pwm_signal = MCPWM1A,
        .pwm_gpio_num = RIGHT_MOTOR_PWM_GPIO,
        .dir_gpio_num = RIGHT_MOTOR_DIR_GPIO,
        .fault_gpio_num = RIGHT_MOTOR_FAULT_GPIO,
        .fault_led_gpio_num = RIGHT_MOTOR_FAULT_LED,
        .pwm_frequency_hz = 20000,
        .ramp_rate = 5,            // Adjust as needed
        .ramp_intervall_ms = 10,   // Adjust as needed
        .direction_hysteresis = 5, // Adjust as needed
        .pwm_duty_limit = 100,
        .mynr = 1};

    // Initialize differential drive
    diff_drive_handle_t *diff_drive = diff_drive_init(&diff_drive_config,
                                                      &left_motor_config,
                                                      &right_motor_config);

    // Initialize the DS4 controller
    ds4_init();

    // Initialize manual control on core 1
    manual_control_init(&manual_control_cfg, diff_drive);

        // Main task can do other work or just wait
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}