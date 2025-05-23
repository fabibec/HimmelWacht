#include "diff-drive.h"
#include "esp_log.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "log_wrapper.h"

#define TAG "DIFF_DRIVE"

typedef struct diff_drive_cmd
{
    float left_speed;            // Left motor speed (0-100)
    float right_speed;           // Right motor speed (0-100)
    motor_direction_t left_dir;  // Left motor direction
    motor_direction_t right_dir; // Right motor direction
} diff_drive_cmd_t;

// Helper function to calculate motor speeds from x, y inputs
static void calculate_speeds(int16_t x, int16_t y, int16_t *max_input, float *left_limit, float *right_limit, float *left_speed, float *right_speed,
                             motor_direction_t *left_dir, motor_direction_t *right_dir);
esp_err_t create_task(diff_drive_handle_t *handle, uint8_t priority);
static void diff_drive_task(void *pvParameters);

diff_drive_handle_t *diff_drive_init(const diff_drive_config_t *config, const motor_config_t *left_motor_config, const motor_config_t *right_motor_config)
{
    // Input validation
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Differential drive config is NULL");
        return NULL;
    }

    // Check if motors are configured
    if (left_motor_config == NULL || right_motor_config == NULL)
    {
        ESP_LOGE(TAG, "Motor configurations are NULL");
        return NULL;
    }

    // Create handle
    diff_drive_handle_t *diff_drive = (diff_drive_handle_t *)calloc(1, sizeof(diff_drive_handle_t));
    if (!diff_drive)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for differential drive handle");
        return NULL;
    }

    // Store configuration
    memcpy(&diff_drive->config, config, sizeof(diff_drive_config_t));

    // Initialize motors
    diff_drive->left_motor = motor_driver_init(left_motor_config);
    if (!diff_drive->left_motor)
    {
        ESP_LOGE(TAG, "Failed to initialize left motor");
        motor_driver_deinit(diff_drive->left_motor);
        free(diff_drive);
        return NULL;
    }

    diff_drive->right_motor = motor_driver_init(right_motor_config);
    if (!diff_drive->right_motor)
    {
        ESP_LOGE(TAG, "Failed to initialize right motor");
        motor_driver_deinit(diff_drive->right_motor);
        free(diff_drive);
        return NULL;
    }

    diff_drive->cmd_queue = xQueueCreate(config->cmd_queue_size, sizeof(diff_drive_cmd_t));
    if (diff_drive->cmd_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create command queue");
        motor_driver_deinit(diff_drive->left_motor);
        motor_driver_deinit(diff_drive->right_motor);
        free(diff_drive);
        return NULL;
    }

    diff_drive->initialized = true;

    ESP_LOGI(TAG, "Differential drive initialized successfully");

    esp_err_t ret = create_task(diff_drive, config->task_priority);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create differential drive task");
        motor_driver_deinit(diff_drive->left_motor);
        motor_driver_deinit(diff_drive->right_motor);
        vQueueDelete(diff_drive->cmd_queue);
        free(diff_drive);
        return NULL;
    }

    return diff_drive;
}

esp_err_t create_task(diff_drive_handle_t *handle, uint8_t priority)
{
    if (handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Create task
    BaseType_t ret = xTaskCreatePinnedToCore(
        diff_drive_task,
        "diff_drive_task",
        handle->config.task_stack_size, // Stack size
        handle,                         // Task parameter
        handle->config.task_priority,
        &handle->task_handle,
        handle->config.task_core_id);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create differential drive task");
        return ESP_FAIL;
    }

    handle->is_running = true;
    ESP_LOGI(TAG, "Differential drive task started");

    return ESP_OK;
}

esp_err_t diff_drive_send_cmd(diff_drive_handle_t *diff_drive, input_matrix_t *matrix)
{
    // Input validation
    if (diff_drive == NULL || matrix == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!diff_drive->is_running)
    {
        ESP_LOGE(TAG, "Differential drive task is not running");
        return ESP_ERR_INVALID_STATE;
    }

    // Calculate motor speeds and directions based on x, y inputs
    diff_drive_cmd_t cmd;

    calculate_speeds(matrix->x, matrix->y, &diff_drive->config.max_input, &diff_drive->left_motor->config.pwm_duty_limit, &diff_drive->right_motor->config.pwm_duty_limit, &cmd.left_speed, &cmd.right_speed, &cmd.left_dir, &cmd.right_dir);

    LOGI(TAG, "Sending command: left_speed=%.2f, right_speed=%.2f, left_dir=%d, right_dir=%d",
         cmd.left_speed, cmd.right_speed, cmd.left_dir, cmd.right_dir);

    // Send command to queue
    if (xQueueSend(diff_drive->cmd_queue, &cmd, diff_drive->config.queue_timout_ms) != pdTRUE)
    {
        ESP_LOGW(TAG, "Failed to send command to queue, queue might be full");
        return ESP_ERR_TIMEOUT;
    }

    LOGI(TAG, "Command sent to queue: left_speed=%.2f, right_speed=%.2f, left_dir=%d, right_dir=%d",
         cmd.left_speed, cmd.right_speed, cmd.left_dir, cmd.right_dir);

    return ESP_OK;
}

esp_err_t diff_drive_update(diff_drive_handle_t *diff_drive)
{
    // Input validation
    if (diff_drive == NULL || !diff_drive->initialized)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t left = ESP_OK;
    esp_err_t right = ESP_OK;

    // Update motor drivers
    if (motor_driver_is_update_necessary(diff_drive->left_motor))
    {
        LOGI(TAG, "Left update detected");
        left = motor_driver_update(diff_drive->left_motor);
    }

    if (motor_driver_is_update_necessary(diff_drive->right_motor))
    {
        LOGI(TAG, "Right update detected");
        right = motor_driver_update(diff_drive->right_motor);
    }

    return left == ESP_OK && right == ESP_OK ? ESP_OK : ESP_FAIL;
}

static void diff_drive_task(void *pvParameters)
{
    diff_drive_handle_t *drive = (diff_drive_handle_t *)pvParameters;
    diff_drive_cmd_t cmd;

    // Main task loop
    while (1)
    {
        // // Check for fault condition
        // if (motor_driver_is_fault_active(drive->left_motor) ||
        //     motor_driver_is_fault_active(drive->right_motor))
        // {

        //     // Emergency stop
        //     motor_driver_set_speed(drive->left_motor, 0, MOTOR_DIRECTION_STOP);
        //     motor_driver_set_speed(drive->right_motor, 0, MOTOR_DIRECTION_STOP);
        //     motor_driver_update(drive->left_motor);
        //     motor_driver_update(drive->right_motor);

        //     // Log fault state
        //     ESP_LOGE(TAG, "Fault detected: left=%d, right=%d",
        //              motor_driver_is_fault_active(drive->left_motor),
        //              motor_driver_is_fault_active(drive->right_motor));

        //     // Wait for recovery delay
        //     vTaskDelay(pdMS_TO_TICKS(drive->config.recovery_time_ms));

        //     // Attempt to clear faults
        //     if (motor_driver_is_fault_active(drive->left_motor))
        //     {
        //         ESP_LOGI(TAG, "Attempting to clear left motor fault");
        //         motor_driver_clear_fault(drive->left_motor);
        //     }

        //     if (motor_driver_is_fault_active(drive->right_motor))
        //     {
        //         ESP_LOGI(TAG, "Attempting to clear right motor fault");
        //         motor_driver_clear_fault(drive->right_motor);
        //     }

        //     // Continue to next iteration
        //     continue;
        // }

        // Process commands from queue pdMS_TO_TICKS(drive->config.queue_timout_ms)
        if (xQueueReceive(drive->cmd_queue, &cmd, drive->config.queue_timout_ms) == pdTRUE)
        {
            // Set motor speeds and directions
            motor_driver_set_speed(drive->left_motor, cmd.left_speed, cmd.left_dir);
            motor_driver_set_speed(drive->right_motor, cmd.right_speed, cmd.right_dir);

            // Log command
            ESP_LOGI(TAG, "Command received: left_speed=%.2f, right_speed=%.2f, left_dir=%d, right_dir=%d",
                 cmd.left_speed, cmd.right_speed, cmd.left_dir, cmd.right_dir);
        }

        // Update motors
        esp_err_t ret = diff_drive_update(drive);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to update motors: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(drive->config.task_delay_ms)); // Adjust delay as needed
    }
}

esp_err_t diff_drive_deinit(diff_drive_handle_t *diff_drive)
{
    // Input validation
    if (diff_drive == NULL || !diff_drive->initialized)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Delete queue
    if (diff_drive->cmd_queue != NULL)
    {
        vQueueDelete(diff_drive->cmd_queue);
    }

    if (diff_drive->task_handle != NULL)
    {
        vTaskDelete(diff_drive->task_handle);
    }

    // Deinitialize motors
    motor_driver_deinit(diff_drive->left_motor);
    motor_driver_deinit(diff_drive->right_motor);

    // Free memory
    free(diff_drive);
    diff_drive = NULL;

    ESP_LOGI(TAG, "Differential drive deinitialized");
    return ESP_OK;
}

static void calculate_speeds(int16_t x, int16_t y, int16_t *max_input, float *left_limit, float *right_limit, float *left_speed, float *right_speed,
                             motor_direction_t *left_dir, motor_direction_t *right_dir)
{
    // check stop
    if (x == 0 && y == 0)
    {
        *left_speed = 0.0f;
        *right_speed = 0.0f;
        *left_dir = MOTOR_DIRECTION_STOP;
        *right_dir = MOTOR_DIRECTION_STOP;
        return;
    }

    // Normalize inputs to -1.0 to 1.0 range
    float h_norm = (float)x / (float)*max_input;
    float v_norm = (float)y / (float)*max_input;

    // Clamp values to ensure they stay within bounds
    if (h_norm > 1.0f)
        h_norm = 1.0f;
    if (h_norm < -1.0f)
        h_norm = -1.0f;
    if (v_norm > 1.0f)
        v_norm = 1.0f;
    if (v_norm < -1.0f)
        v_norm = -1.0f;

    float left = 0.0f;
    float right = 0.0f;

    //handle straight forward and backward movement
    if(fabsf(h_norm) < 0.20f)
    {
        h_norm = 0.0f;
    }
    //handle straight left and right movement for in place rotation
    if(fabsf(v_norm) < 0.20f)
    {
        v_norm = 0.0f;
    }


    // Zero or near-zero vertical input - rotate in place
    if (fabsf(v_norm) == 0)
    {
        // Sharp turn mode - use opposing wheel directions
        left = fabsf(h_norm) * 100.0f;
        right = fabsf(h_norm) * 100.0f;

        if (h_norm > 0)
        {
            // Turning right (clockwise)
            *left_dir = MOTOR_DIRECTION_FORWARD;
            *right_dir = MOTOR_DIRECTION_BACKWARD;
        }
        else if (h_norm < 0)
        {
            // Turning left (counter-clockwise)
            *left_dir = MOTOR_DIRECTION_BACKWARD;
            *right_dir = MOTOR_DIRECTION_FORWARD;
        }
        else
        {
            // No movement
            *left_dir = MOTOR_DIRECTION_STOP;
            *right_dir = MOTOR_DIRECTION_STOP;
            left = 0.0f;
            right = 0.0f;
        }
    }
    else
    {
        // Set base directions according to vertical input
        if (v_norm > 0)
        {
            *left_dir = MOTOR_DIRECTION_FORWARD;
            *right_dir = MOTOR_DIRECTION_FORWARD;
        }
        else
        {
            *left_dir = MOTOR_DIRECTION_BACKWARD;
            *right_dir = MOTOR_DIRECTION_BACKWARD;
        }

        // Get absolute value of vertical input for base speed
        float base_speed = fabsf(v_norm);

        // Apply turning factor
        if (h_norm != 0)
        {
            // Calculate turn influence (0.0 to 1.0)
            float turn_factor = fabsf(h_norm);

            // Apply turn factor to appropriate wheel
            if (h_norm > 0)
            {
                // Turning right - slow down right wheel
                left = base_speed * 100.0f;
                right = base_speed * (1.0f - turn_factor) * 100.0f;

                // If sharp turn and strong horizontal input, reverse the inner wheel
                if (turn_factor > 0.7f)
                {
                    right = turn_factor * 50.0f; // Scale for reasonable reverse speed
                    if (*right_dir == MOTOR_DIRECTION_FORWARD)
                        *right_dir = MOTOR_DIRECTION_BACKWARD;
                    else
                        *right_dir = MOTOR_DIRECTION_FORWARD;
                }
            }
            else
            {
                // Turning left - slow down left wheel
                left = base_speed * (1.0f - turn_factor) * 100.0f;
                right = base_speed * 100.0f;

                // If sharp turn and strong horizontal input, reverse the inner wheel
                if (turn_factor > 0.7f)
                {
                    left = turn_factor * 50.0f; // Scale for reasonable reverse speed
                    if (*left_dir == MOTOR_DIRECTION_FORWARD)
                        *left_dir = MOTOR_DIRECTION_BACKWARD;
                    else
                        *left_dir = MOTOR_DIRECTION_FORWARD;
                }
            }
        }
        else
        {
            // Straight movement
            left = base_speed * 100.0f;
            right = base_speed * 100.0f;
        }
    }

    // Clamp speeds to [0, 100]
    if (left > 100.0f)
        left = 100.0f;
    if (left < 0.0f)
        left = 0.0f;
    if (right > 100.0f)
        right = 100.0f;
    if (right < 0.0f)
        right = 0.0f;

    // Scale them to 0-max_pwm
    *left_speed = (left / 100.0f) * *left_limit;
    *right_speed = (right / 100.0f) * *right_limit;
}

void diff_drive_print_all_parameters(diff_drive_handle_t *diff_drive)
{
    if (diff_drive == NULL)
    {
        ESP_LOGE(TAG, "Differential drive is NULL");
        return;
    }

    ESP_LOGI(TAG, "Differential Drive Parameters:");
    ESP_LOGI(TAG, "  Left Motor: ");
    motor_driver_print_all_parameters(diff_drive->left_motor);
    ESP_LOGI(TAG, "  Right Motor: ");
    motor_driver_print_all_parameters(diff_drive->right_motor);
}