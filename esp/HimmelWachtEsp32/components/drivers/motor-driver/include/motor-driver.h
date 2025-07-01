/**
 * @file motor-driver.h
 * @brief Motor Driver for ESP32 using MCPWM
 * 
 * This driver provides an interface to control motors using the MCPWM peripheral of the ESP32.
 * It supports setting speed, direction, and ramping up/down the motor speed.
 * It also includes emergency stop functionality and parameter printing for debugging.
 * 
 * Reference:
 *  - https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/mcpwm.html
 * 
 * @author Michael Specht
 */

#pragma once

#include "esp_err.h"
#include "driver/mcpwm.h"

typedef enum
{
    MOTOR_DIRECTION_FORWARD,
    MOTOR_DIRECTION_BACKWARD,
    MOTOR_DIRECTION_STOP
} motor_direction_t;

typedef struct
{
    mcpwm_unit_t mcpwm_unit;
    mcpwm_timer_t timer_num;
    mcpwm_operator_t generator;
    mcpwm_io_signals_t pwm_signal;
    uint8_t pwm_gpio_num;
    uint8_t dir_gpio_num;
    uint16_t pwm_frequency_hz;
    uint8_t ramp_rate;
    uint8_t ramp_intervall_ms;
    uint8_t direction_hysteresis;
    float pwm_duty_limit;
    uint8_t mynr;
} motor_config_t;


typedef struct
{
    float current_pwm;                   // current speed(0-100)
    float target_pwm;                    // target speed
    motor_direction_t current_direction; 
    motor_direction_t target_direction;
    uint32_t last_update_ms;
    motor_config_t config;
    bool initialized;
} motor_handle_t;

/**
 * @brief Initialize the motor driver with the given configuration.
 *
 * @param config Pointer to the motor configuration structure.
 * @return Pointer to the motor handle on success, NULL on failure.
 */
motor_handle_t *motor_driver_init(const motor_config_t *config);

/**
 * @brief Set the speed and direction of the motor.
 *
 * @param motor Pointer to the motor handle.
 * @param duty_cycle The desired duty cycle (0.0 to 100.0).
 * @param direction The desired direction of the motor.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t motor_driver_set_speed(motor_handle_t *motor, float duty_cycle, motor_direction_t direction);

/**
 * @brief Update the motor state based on the current configuration and target values.
 * 
 * @param motor Pointer to the motor handle.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t motor_driver_update(motor_handle_t *motor);

/**
 * @brief Emergency stop the motor by setting PWM to 0 and direction to stop.
 *
 * @param motor Pointer to the motor handle.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t motor_driver_emergency_stop(motor_handle_t *motor);

/**
 * @brief Deinitialize the motor driver and free resources.
 *
 * @param motor Pointer to the motor handle.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t motor_driver_deinit(motor_handle_t *motor);

/**
 * @brief Print all parameters of the motor for debugging purposes.
 *
 * @param motor Pointer to the motor handle.
 */
void motor_driver_print_all_parameters(motor_handle_t *motor);

/**
 * @brief Check if an update to the motor is necessary based on the target and current values.
 *
 * @param motor Pointer to the motor handle.
 * @return true if an update is necessary, false otherwise.
 */
bool motor_driver_is_update_necessary(motor_handle_t *motor);