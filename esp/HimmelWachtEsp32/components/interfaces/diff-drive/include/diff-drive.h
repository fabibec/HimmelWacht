/**
 * @file diff-drive.h
 * @brief Differential drive interface for controlling two motors
 * 
 * This header file defines the differential drive interface for controlling two motors
 * using a queue for command handling. It includes the necessary structures, function declarations,
 * and configuration options.
 * 
 * Reference:
 *  - https://components.espressif.com/components/espressif/iqmath
 * 
 * @author Michael Specht
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "motor-driver.h"

typedef struct
{
    float left_duty;
    float right_duty;
} pwm_output_t;

typedef struct input_matrix
{
    uint16_t x;
    uint16_t y;
} input_matrix_t;

typedef struct
{
    int16_t max_input; // e.g., 512
    uint32_t cmd_queue_size;
    uint32_t recovery_time_ms;
    uint8_t task_priority;
    uint32_t task_stack_size;
    uint8_t task_core_id;
    uint8_t task_delay_ms;
    uint32_t queue_timout_ms;
} diff_drive_config_t;

typedef struct
{
    motor_handle_t *left_motor;
    motor_handle_t *right_motor;
    bool initialized;
    bool is_running;
    diff_drive_config_t config;
    QueueHandle_t cmd_queue;
    TaskHandle_t task_handle;
} diff_drive_handle_t;

/**
 * @brief Initialize the differential drive with the given configuration and motor configurations
 *
 * @param config Pointer to the differential drive configuration
 * @param left_motor_config Pointer to the left motor configuration
 * @param right_motor_config Pointer to the right motor configuration
 * @return diff_drive_handle_t* Pointer to the initialized differential drive handle, or NULL on failure
 */
diff_drive_handle_t *diff_drive_init(const diff_drive_config_t *config, const motor_config_t *left_motor_config, const motor_config_t *right_motor_config);

/**
 * @brief Send a command to the differential drive
 *
 * @param diff_drive Pointer to the differential drive handle
 * @param matrix Pointer to the input matrix containing x and y values
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if diff_drive or matrix is NULL,
 *                  ESP_ERR_INVALID_STATE if the task is not running, or ESP_ERR_TIMEOUT if the command could not be sent within the timeout period.
 */
esp_err_t diff_drive_send_cmd(diff_drive_handle_t *diff_drive, input_matrix_t *matrix);

/**
 * @brief Print all parameters of the differential drive
 * 
 * @param diff_drive Pointer to the differential drive handle
 */
void diff_drive_print_all_parameters(diff_drive_handle_t *diff_drive);

/**
 * @brief Deinitialize the differential drive and free resources
 *
 * @param diff_drive Pointer to the differential drive handle
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if diff_drive is NULL or not initialized
 */
esp_err_t diff_drive_deinit(diff_drive_handle_t *diff_drive);