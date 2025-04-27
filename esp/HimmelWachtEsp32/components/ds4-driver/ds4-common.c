#include "ds4-common.h"

const uint8_t input_processing_freq_hz = 60; // 1 Hz = 1 Input report per seconds
const uint32_t input_processing_interval_us = 1000000 / input_processing_freq_hz;

const uint8_t low_battery_threshold = 25; // 0 - 254 -> 25 ~ 10% battery left
const uint16_t low_battery_blinking_interval_ms = 1500;

bd_addr_t ds4_address = {0};

const uint8_t output_event_queue_size = 16; // 16 events
