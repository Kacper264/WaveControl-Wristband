#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

void battery_init(void);
float battery_read_voltage(void);
uint8_t battery_read_percent(void);

#ifdef __cplusplus
}
#endif
