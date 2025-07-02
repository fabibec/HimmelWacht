/**
 * @file
 * @brief Firing Control for the shooting platform
 *
 * This code uses a servo to shove the nerf dart into the flywheel, which shoot out the dart.
 * This code uses the PCA9685 PWM Board and my driver to control the servo motor, as well as a MOSFET module to control the flywheel.
 * Since the flywheel only needs to be turned on and off, a GPIO pin is used to control the flywheel.
 *
 * @author Fabian Becker
 */
#ifndef _FIRE_CONTROL_H_
#define _FIRE_CONTROL_H_

#include <stdint.h>
#include <esp_err.h>

// Configuration for the firing control
typedef struct {
    int8_t gun_arm_channel; // Channel on the PWM board for the servo motor of the gun arm
    int8_t flywheel_control_gpio_port; // GPIO port for the flywheel control
    int8_t run_on_core; // Core to run on
} fire_control_config_t;

esp_err_t fire_control_init(fire_control_config_t *cfg);
esp_err_t fire_control_trigger_shot();

#endif //_FIRE_CONTROL_H_
