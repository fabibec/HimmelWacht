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
static void
calculate_speeds(uint16_t x, uint16_t y, int max_input, float *left_speed, float *right_speed,
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

    calculate_speeds(matrix->x, matrix->y, diff_drive->config.max_input, &cmd.left_speed, &cmd.right_speed, &cmd.left_dir, &cmd.right_dir);

    LOGI(TAG, "Sending command: left_speed=%.2f, right_speed=%.2f, left_dir=%d, right_dir=%d",
             cmd.left_speed, cmd.right_speed, cmd.left_dir, cmd.right_dir);

    // Send command to queue
    if (xQueueSend(diff_drive->cmd_queue, &cmd, diff_drive->config.queue_timout_ms) != pdTRUE)
    {
        ESP_LOGW(TAG, "Failed to send command to queue, queue might be full");
        return ESP_ERR_TIMEOUT;
    }

    // ESP_LOGI(TAG, "Command sent to queue: left_speed=%.2f, right_speed=%.2f, left_dir=%d, right_dir=%d",
    //          cmd.left_speed, cmd.right_speed, cmd.left_dir, cmd.right_dir);

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
    if(motor_driver_is_update_necessary(diff_drive->left_motor)){
        LOGI(TAG, "Left update detected");
        left = motor_driver_update(diff_drive->left_motor);
    }

    if(motor_driver_is_update_necessary(diff_drive->right_motor)){
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
            LOGI(TAG, "Command received: left_speed=%.2f, right_speed=%.2f, left_dir=%d, right_dir=%d",
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

    if( diff_drive->task_handle != NULL )
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

/**
 * Helper function to calculate motor speeds from x, y inputs
 *
 * x: Left/Right axis (-max_input to +max_input)
 * y: Forward/Backward axis (-max_input to +max_input)
 *
 * Converts joystick-like input to differential drive motor commands
 */
static void calculate_speeds(uint16_t x, uint16_t y, int max_input, float *left_speed, float *right_speed,
                                        motor_direction_t *left_dir, motor_direction_t *right_dir)
{
    // Constrain inputs to valid range
    if (x > max_input)
        x = max_input;
    if (x < -max_input)
        x = -max_input;
    if (y > max_input)
        y = max_input;
    if (y < -max_input)
        y = -max_input;

    // Default directions
    *left_dir = MOTOR_DIRECTION_FORWARD;
    *right_dir = MOTOR_DIRECTION_FORWARD;

    // Calculate raw speeds (can be negative)
    float left_raw = y + x;
    float right_raw = y - x;

    // Normalize to max value if needed
    float max_raw = max_input;
    if (fabs(left_raw) > max_raw || fabs(right_raw) > max_raw)
    {
        float scaling = max_raw / fmax(fabs(left_raw), fabs(right_raw));
        left_raw *= scaling;
        right_raw *= scaling;
    }

    // Convert to 0-100 range and set directions
    if (left_raw < 0)
    {
        *left_speed = (-left_raw * 100 / max_input);
        *left_dir = MOTOR_DIRECTION_BACKWARD;
    }
    else
    {
        *left_speed = (left_raw * 100 / max_input);
        *left_dir = MOTOR_DIRECTION_FORWARD;
    }

    if (right_raw < 0)
    {
        *right_speed = (-right_raw * 100 / max_input);
        *right_dir = MOTOR_DIRECTION_BACKWARD;
    }
    else
    {
        *right_speed = (right_raw * 100 / max_input);
        *right_dir = MOTOR_DIRECTION_FORWARD;
    }

    // Handle stop case
    if (x == 0 && y == 0)
    {
        *left_speed = 0;
        *right_speed = 0;
        *left_dir = MOTOR_DIRECTION_STOP;
        *right_dir = MOTOR_DIRECTION_STOP;
    }

    // Log calculated speeds
    LOGI(TAG, "Calculated speeds: left=%.2f, right=%.2f, left_dir=%d, right_dir=%d",
             *left_speed, *right_speed, *left_dir, *right_dir);
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

/**
 * Convert PS4 controller inputs to PWM duty cycles for skid steering
 * 
 * @param horizontal -512 (left) to 512 (right)
 * @param vertical -512 (back) to 512 (forward)
 * @return pwm_output_t with left_duty and right_duty (0.0 to 100.0)
 *         Positive values = forward, Negative values = backward
 */
pwm_output_t controller_to_pwm(int16_t horizontal, int16_t vertical) {
    pwm_output_t output;
    
    // Normalize inputs to -1.0 to 1.0 range
    float h_norm = (float)horizontal / 512.0f;
    float v_norm = (float)vertical / 512.0f;
    
    // Clamp values to ensure they stay within bounds
    if (h_norm > 1.0f) h_norm = 1.0f;
    if (h_norm < -1.0f) h_norm = -1.0f;
    if (v_norm > 1.0f) v_norm = 1.0f;
    if (v_norm < -1.0f) v_norm = -1.0f;
    
    // Check for sharp turns (more than 50% stick deflection)
    if (fabsf(h_norm) > 0.5f) {
        // Sharp turn mode - use opposing wheel directions
        if (h_norm > 0) {  // Turning right
            output.left_duty = v_norm * 100.0f;
            output.right_duty = -v_norm * 100.0f;
        } else {  // Turning left
            output.left_duty = -v_norm * 100.0f;
            output.right_duty = v_norm * 100.0f;
        }
    } else {
        // Gentle turn mode - use differential speeds
        float base_speed = v_norm;
        float turn_factor = h_norm * 0.5f;  // Scale down turn influence
        
        output.left_duty = (base_speed - turn_factor) * 100.0f;
        output.right_duty = (base_speed + turn_factor) * 100.0f;
    }
    
    // Clamp final values to [-100.0, 100.0]
    if (output.left_duty > 100.0f) output.left_duty = 100.0f;
    if (output.left_duty < -100.0f) output.left_duty = -100.0f;
    if (output.right_duty > 100.0f) output.right_duty = 100.0f;
    if (output.right_duty < -100.0f) output.right_duty = -100.0f;
    
    return output;
}