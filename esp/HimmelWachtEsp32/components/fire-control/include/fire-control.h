#ifndef _FIRE_CONTROL_H_
#define _FIRE_CONTROL_H_

#include <stdint.h>
#include <esp_err.h>

// Available channels on the PWM Board
typedef enum  {
    channel_0 = 0,
    channel_1,
    channel_2,
    channel_3,
    channel_4,
    channel_5,
    channel_6,
    channel_7,
    channel_8,
    channel_9,
    channel_10,
    channel_11,
    channel_12,
    channel_13,
    channel_14,
    channel_15,
} fire_control_channel_t;

typedef struct {
    fire_control_channel_t gun_arm_channel; // Channel on the PWM board for the servo motor of the gun arm
} fire_control_config_t;

esp_err_t fire_control_init(fire_control_config_t *cfg);
esp_err_t fire_control_trigger_shot();

#endif //_FIRE_CONTROL_H_
