#include "diff-drive.h"
#include "esp_log.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "log_wrapper.h"
#include <IQmathLib.h>

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

static void calculate_speeds(int16_t x, int16_t y, int16_t *max_input, 
                               float *left_limit, float *right_limit, 
                               float *left_speed, float *right_speed,
                               motor_direction_t *left_dir, motor_direction_t *right_dir)
{
    // Convert constants to IQ format
    const _iq IQ_ZERO = _IQ(0.0);
    const _iq IQ_ONE = _IQ(1.0);
    const _iq IQ_MINUS_ONE = _IQ(-1.0);
    const _iq IQ_DEADBAND = _IQ(0.20);
    const _iq IQ_SHARP_TURN_THRESHOLD = _IQ(0.7);
    const _iq IQ_HUNDRED = _IQ(100.0);
    const _iq IQ_FIFTY = _IQ(50.0);
    
    // Convert float inputs to IQ format
    _iq left_limit_iq = _IQ(*left_limit);
    _iq right_limit_iq = _IQ(*right_limit);
    
    // Check stop condition
    if (x == 0 && y == 0)
    {
        *left_speed = 0.0f;
        *right_speed = 0.0f;
        *left_dir = MOTOR_DIRECTION_STOP;
        *right_dir = MOTOR_DIRECTION_STOP;
        return;
    }

    // Convert max_input to IQ format
    _iq max_input_iq = _IQ((float)*max_input);
    
    // Normalize inputs to -1.0 to 1.0 range
    _iq h_norm = _IQdiv(_IQ((float)x), max_input_iq);
    _iq v_norm = _IQdiv(_IQ((float)y), max_input_iq);

    // Clamp values to ensure they stay within bounds
    if (h_norm > IQ_ONE)
        h_norm = IQ_ONE;
    if (h_norm < IQ_MINUS_ONE)
        h_norm = IQ_MINUS_ONE;
    if (v_norm > IQ_ONE)
        v_norm = IQ_ONE;
    if (v_norm < IQ_MINUS_ONE)
        v_norm = IQ_MINUS_ONE;

    _iq left = IQ_ZERO;
    _iq right = IQ_ZERO;

    // Handle straight forward and backward movement (deadband)
    if (_IQabs(h_norm) < IQ_DEADBAND)
    {
        h_norm = IQ_ZERO;
    }
    
    // Handle straight left and right movement for in place rotation (deadband)
    if (_IQabs(v_norm) < IQ_DEADBAND)
    {
        v_norm = IQ_ZERO;
    }

    // Zero or near-zero vertical input - rotate in place
    if (_IQabs(v_norm) == IQ_ZERO)
    {
        // Sharp turn mode - use opposing wheel directions
        left = _IQmpy(_IQabs(h_norm), IQ_HUNDRED);
        right = _IQmpy(_IQabs(h_norm), IQ_HUNDRED);

        if (h_norm > IQ_ZERO)
        {
            // Turning right (clockwise)
            *left_dir = MOTOR_DIRECTION_FORWARD;
            *right_dir = MOTOR_DIRECTION_BACKWARD;
        }
        else if (h_norm < IQ_ZERO)
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
            left = IQ_ZERO;
            right = IQ_ZERO;
        }
    }
    else
    {
        // Set base directions according to vertical input
        if (v_norm > IQ_ZERO)
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
        _iq base_speed = _IQabs(v_norm);

        // Apply turning factor
        if (h_norm != IQ_ZERO)
        {
            // Calculate turn influence (0.0 to 1.0)
            _iq turn_factor = _IQabs(h_norm);

            // Apply turn factor to appropriate wheel
            if (h_norm > IQ_ZERO)
            {
                // Turning right - slow down right wheel
                left = _IQmpy(base_speed, IQ_HUNDRED);
                right = _IQmpy(_IQmpy(base_speed, (IQ_ONE - turn_factor)), IQ_HUNDRED);

                // If sharp turn and strong horizontal input, reverse the inner wheel
                if (turn_factor > IQ_SHARP_TURN_THRESHOLD)
                {
                    right = _IQmpy(turn_factor, IQ_FIFTY); // Scale for reasonable reverse speed
                    if (*right_dir == MOTOR_DIRECTION_FORWARD)
                        *right_dir = MOTOR_DIRECTION_BACKWARD;
                    else
                        *right_dir = MOTOR_DIRECTION_FORWARD;
                }
            }
            else
            {
                // Turning left - slow down left wheel
                left = _IQmpy(_IQmpy(base_speed, (IQ_ONE - turn_factor)), IQ_HUNDRED);
                right = _IQmpy(base_speed, IQ_HUNDRED);

                // If sharp turn and strong horizontal input, reverse the inner wheel
                if (turn_factor > IQ_SHARP_TURN_THRESHOLD)
                {
                    left = _IQmpy(turn_factor, IQ_FIFTY); // Scale for reasonable reverse speed
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
            left = _IQmpy(base_speed, IQ_HUNDRED);
            right = _IQmpy(base_speed, IQ_HUNDRED);
        }
    }

    // Clamp speeds to [0, 100]
    if (left > IQ_HUNDRED)
        left = IQ_HUNDRED;
    if (left < IQ_ZERO)
        left = IQ_ZERO;
    if (right > IQ_HUNDRED)
        right = IQ_HUNDRED;
    if (right < IQ_ZERO)
        right = IQ_ZERO;

    // Scale them to 0-max_pwm and convert back to float
    *left_speed = _IQtoF(_IQmpy(_IQdiv(left, IQ_HUNDRED), left_limit_iq));
    *right_speed = _IQtoF(_IQmpy(_IQdiv(right, IQ_HUNDRED), right_limit_iq));
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