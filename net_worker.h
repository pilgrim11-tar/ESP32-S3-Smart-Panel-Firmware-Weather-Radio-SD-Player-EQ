#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

struct NetWorkerHttpOptions
{
    bool insecure_tls = true;
    uint32_t connect_timeout_ms = 5000;
    uint32_t timeout_ms = 7000;
    bool disable_redirects = false;
};

bool net_worker_begin(UBaseType_t task_priority = 1, BaseType_t core_id = 1);
bool net_worker_is_ready();
int net_worker_http_get(const String &url, String *response_body, const NetWorkerHttpOptions *options = nullptr, uint32_t wait_timeout_ms = 20000);
