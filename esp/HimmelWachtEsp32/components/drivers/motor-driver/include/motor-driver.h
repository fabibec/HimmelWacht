#pragma once

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
 * @brief Motor configuration structure
 *
 * This structure holds the configuration parameters for the motor driver,
 * including the MCPWM unit, timer, generator, GPIO pins, and PWM frequency.
 */
typedef struct
{
    mcpwm_unit_t mcpwm_unit;       // MCPWM unit (0 or 1)
    mcpwm_timer_t timer_num;       // Timer number
    mcpwm_operator_t generator;   // MCPWM generator
    mcpwm_io_signals_t pwm_signal; // PWM signal
    uint8_t pwm_gpio_num;          // GPIO for PWM output
    uint8_t dir_gpio_num;          // GPIO for direction output
    uint16_t pwm_frequency_hz;     // PWM frequency in Hz
    uint8_t ramp_rate;
    uint8_t ramp_intervall_ms;
    uint8_t direction_hysteresis; // Hysteresis for direction change
    float pwm_duty_limit;
    uint8_t mynr;
} motor_config_t;

/**
 * @brief Motor handle structure
 *
 * This structure holds the state of the motor, including its current speed,
 * target speed, and direction. It is used to manage the motor's operation.
 */
typedef struct
{
    float current_pwm;                   // Current speed (0-100)
    float target_pwm;                    // Target speed
    motor_direction_t current_direction; // Current direction
    motor_direction_t target_direction;  // Requested new direction
    uint32_t last_update_ms;             // Time when the state was entered
    motor_config_t config;               // Motor configuration
    bool initialized;                    // Flag to indicate if the motor is initialized
} motor_handle_t;

/**
 * @brief Initialize the motor driver
 *
 * @param config Pointer to the motor configuration structure
 * @return motor_handle_t* Pointer to the motor handle, NULL on error
 */
motor_handle_t *motor_driver_init(const motor_config_t *config);

/**
 * @brief Set the speed and direction of the motor
 *
 * @param motor Pointer to the motor handle
 * @param duty_cycle Duty cycle (0-100)
 * @param direction Motor direction
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t motor_driver_set_speed(motor_handle_t *motor, float duty_cycle, motor_direction_t direction);

/**
 * @brief Applies the target speed and direction to the motor
 *
 * This function should be called periodically to update the motor state.
 * It handles ramping up and down the speed based on the configured ramp rate.
 * @param motor Pointer to the motor handle
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t motor_driver_update(motor_handle_t *motor);

/**
 * @brief Set the motor to emergency stop
 *
 * This function sets the motor to a safe state in case of an emergency.
 * It should be called when a fault is detected or when immediate stopping is required.
 * @param motor Pointer to the motor handle
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t motor_driver_emergency_stop(motor_handle_t *motor);

/**
 * @brief Stop the motor
 *
 * @param motor Pointer to the motor handle
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t motor_driver_deinit(motor_handle_t *motor);

void motor_driver_print_all_parameters(motor_handle_t *motor);

bool motor_driver_is_update_necessary(motor_handle_t *motor);