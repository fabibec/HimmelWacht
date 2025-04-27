#include "ds4-common.h"

const uint8_t input_processing_freq_hz = 60; // 1 Hz = 1 Input report per seconds
const uint16_t input_processing_interval_us = 1000000 / input_processing_freq_hz;

bd_addr_t ds4_address = {0}; //{0x10, 0x06, 0x1c, 0x68, 0x57, 0x0e};

const uint8_t output_event_queue_size = 16; // 16 events
