#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t websocket_client_start(const char *uri);
esp_err_t websocket_client_send_audio(const uint8_t *data, size_t size);
esp_err_t websocket_client_send_mcp(const char *json_msg);

#ifdef __cplusplus
}
#endif
