#pragma once

#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

void mqtt_init(void);
esp_mqtt_client_handle_t mqtt_get_client(void);

#ifdef __cplusplus
}
#endif
