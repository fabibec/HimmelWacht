#pragma once

#include "freertos/FreeRTOS.h" // <-- ADD THIS
#include "esp_err.h"
#include "driver/mcpwm.h"

/**
 * @brief Motor direction enumeration
 */
typedef enum
{
    MOTOR_DIRECTION_FORWARD,
    MOTOR_DIRECTION_BACKWARD,
    MOTOR_DIRECTION_STOP
} motor_direction_t;

/**
 * @brief Configuration for a single motor
 */
typedef struct
{
    mcpwm_unit_t mcpwm_unit;       // MCPWM unit (0 or 1)
    mcpwm_timer_t timer_num;       // Timer number
    mcpwm_generator_t generator;   // MCPWM generator
    mcpwm_io_signals_t pwm_signal; // PWM signal
    unsigned char pwm_gpio_num;    // GPIO for PWM output
    unsigned char dir_gpio_num;    // GPIO for direction output
    int pwm_frequency_hz;          // PWM frequency in Hz
    unsigned char ramp_rate;
} motor_config_t;

typedef enum
{
    MOTOR_STATE_IDLE,
    MOTOR_STATE_CHANGING_DIRECTION,
    MOTOR_STATE_RUNNING,
} motor_state_t;

typedef struct
{
    unsigned char current_speed;         // Current speed (0-100)
    unsigned char target_speed;          // Target speed
    motor_direction_t current_direction; // Current direction
    motor_direction_t target_direction;  // Requested new direction
    motor_state_t state;                 // Current internal state
    TickType_t state_start_time;         // Time when the state was entered
    motor_config_t config;
    bool initialized;
} motor_handle_t;

/**
 * @brief Initialize the motor driver
 *
 * @param config Motor driver configuration
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
motor_handle_t *motor_driver_init(const motor_config_t *config);

esp_err_t motor_driver_set_speed(motor_handle_t *motor, unsigned char duty_cycle, motor_direction_t direction);

esp_err_t motor_driver_update(motor_handle_t *motor);

/**
 * @brief Deinitialize the motor driver and free resources
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t motor_driver_deinit(motor_handle_t *motor);