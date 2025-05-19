#include "sdkconfig.h"

#include "platform-control.h"
#include "fire-control.h"
#include "ds4-driver.h"
#include "manual-control.h"

#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#define PWM_CHANNEL 0

void app_main(void) {
    pca9685_config_t pwm_board_cfg = {
        .device_address = 0x40,
        .freq = 50,
        .i2c_port = 0,
        .sda_port = 18,
        .scl_port = 19,
        .internal_pullup = true,
    };

    platform_config_t platform_cfg = {
        .pwm_board_config = pwm_board_cfg,
        .platform_x_channel = 2,
        .platform_x_start_angle = 0,
        .platform_x_left_stop_angle = -90,
        .platform_x_right_stop_angle = 90,
        .platform_y_channel = 1,
        .platform_y_start_angle = 47,
        .platform_y_left_stop_angle = 0,
        .platform_y_right_stop_angle = 80
    };

    manual_control_config_t manual_control_cfg = {
        .button_hold_threshold_us = 1500000, // 1.5 seconds
        .max_deg_per_sec = 150,
        .input_processing_freq_hz = 60,
        .deadzone = 30,
        .core = 1
    };

    platform_init(&platform_cfg);
    ESP_LOGI("Platform", "Platform initialized");

    fire_control_config_t fire_control_cfg = {
        .gun_arm_channel = PWM_CHANNEL, // Set the channel for the gun arm
        .flywheel_control_gpio_port = 5, // Set the GPIO port for the flywheel control
        .run_on_core = 1 // Set the core to run on
    };

    fire_control_init(&fire_control_cfg);

    // Initialize the DS4 controller
    ds4_init();

    // Initialize manual control on core 1
    manual_control_init(&manual_control_cfg);
}

// Task to run the test sequence
void test_sequence_task(void *pvParameters)
{
    diff_drive_handle_t *diff_drive = (diff_drive_handle_t *)pvParameters;

    // Give system time to stabilize
    vTaskDelay(pdMS_TO_TICKS(10000));

    // 1. Drive forward for 5 seconds
    send_movement_command(diff_drive, CENTER_VALUE, FORWARD_VALUE, "FORWARD");
    vTaskDelay(pdMS_TO_TICKS(MOVEMENT_TIME_MS));

    // 2. Drive backward for 5 seconds
    send_movement_command(diff_drive, CENTER_VALUE, BACKWARD_VALUE, "BACKWARD");
    vTaskDelay(pdMS_TO_TICKS(MOVEMENT_TIME_MS));

    // 3. Drive forward with left steering for 5 seconds
    send_movement_command(diff_drive, LEFT_TURN_VALUE, FORWARD_VALUE, "FORWARD LEFT");
    vTaskDelay(pdMS_TO_TICKS(MOVEMENT_TIME_MS));

    // 4. Drive backward for 5 seconds
    send_movement_command(diff_drive, CENTER_VALUE, BACKWARD_VALUE, "BACKWARD");
    vTaskDelay(pdMS_TO_TICKS(MOVEMENT_TIME_MS));

    // 5. Drive forward with right steering for 5 seconds
    send_movement_command(diff_drive, RIGHT_TURN_VALUE, FORWARD_VALUE, "FORWARD RIGHT");
    vTaskDelay(pdMS_TO_TICKS(MOVEMENT_TIME_MS));

    // 6. Drive backward for 5 seconds
    send_movement_command(diff_drive, CENTER_VALUE, BACKWARD_VALUE, "BACKWARD");
    vTaskDelay(pdMS_TO_TICKS(MOVEMENT_TIME_MS));

    // Stop the motors
    send_movement_command(diff_drive, CENTER_VALUE, CENTER_VALUE, "STOP");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Test complete
    ESP_LOGI(TAG, "Differential drive test complete");

    // Properly deinitialize
    ESP_LOGI(TAG, "Deinitializing differential drive");
    diff_drive_deinit(diff_drive);

    // Delete this task
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Differential Drive Test Starting");

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
        .pwm_duty_limit = 50,
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
        .pwm_duty_limit = 50,
        .mynr = 1};

    // Initialize differential drive
    diff_drive_handle_t *diff_drive = diff_drive_init(&diff_drive_config,
                                                      &left_motor_config,
                                                      &right_motor_config);

    if (diff_drive == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize differential drive");
        return;
    }

    ESP_LOGI(TAG, "Differential drive initialized successfully");

    diff_drive_print_all_parameters(diff_drive);

    uint8_t core_id = 1;

    // // Create a separate task for the test sequence
    xTaskCreatePinnedToCore(test_sequence_task, "test_sequence", 4096, diff_drive, 4, NULL, core_id);

    // Main task can do other work or just wait
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}