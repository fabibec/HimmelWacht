#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "motor-driver.h"

// Structure to hold the PWM duty cycles
typedef struct {
    float left_duty;
    float right_duty;
} pwm_output_t;

typedef struct input_matrix
{
    uint16_t x; // X-axis input (-max_input to +max_input)
    uint16_t y; // Y-axis input (-max_input to +max_input)
} input_matrix_t;

/**
 * @brief Configuration for differential drive
 */
typedef struct
{
    int16_t max_input;                 // Maximum input value (e.g., 512)
    uint32_t cmd_queue_size;      // Size of command queue
    uint32_t recovery_time_ms; // Recovery time in milliseconds
    uint8_t task_priority;         // Task priority
    uint32_t task_stack_size;         // Stack size for the task
    uint8_t task_core_id;            // Core ID for the task
    uint8_t task_delay_ms;           // Delay in milliseconds for the task
    uint32_t queue_timout_ms; // Timeout for queue operations
} diff_drive_config_t;

/**
 * @brief Differential drive handle
 */
typedef struct
{
    motor_handle_t *left_motor;  // Left motor handle
    motor_handle_t *right_motor; // Right motor handle
    bool initialized;            // Initialization flag
    bool is_running;           // Running flag
    diff_drive_config_t config; // Configuration
    QueueHandle_t cmd_queue;
    TaskHandle_t task_handle; // Task handle for the differential drive
} diff_drive_handle_t;

/**
 * @brief Initialize the differential drive
 *
 * @param config Differential drive configuration
 * @return diff_drive_handle_t* Handle to the differential drive, NULL on failure
 */
diff_drive_handle_t *diff_drive_init(const diff_drive_config_t *config, const motor_config_t *left_motor_config, const motor_config_t *right_motor_config);

/**
 * @brief Set the differential drive speeds based on joystick-like input
 *
 * @param diff_drive Differential drive handle
 * @param x X-axis input (-max_input to +max_input), negative = left, positive = right
 * @param y Y-axis input (-max_input to +max_input), negative = backward, positive = forward
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t diff_drive_send_cmd(diff_drive_handle_t *diff_drive, input_matrix_t *matrix);

void diff_drive_print_all_parameters(diff_drive_handle_t *diff_drive);

/**
 * @brief Deinitialize the differential drive and free resources
 *
 * @param diff_drive Differential drive handle
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t diff_drive_deinit(diff_drive_handle_t *diff_drive);