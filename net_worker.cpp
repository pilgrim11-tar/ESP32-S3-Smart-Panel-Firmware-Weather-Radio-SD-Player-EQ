#include "net_worker.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

namespace
{
struct NetWorkerRequest
{
    String url;
    NetWorkerHttpOptions options;
    String *response_body = nullptr;
    int http_code = -1;
    SemaphoreHandle_t done = nullptr;
};

QueueHandle_t s_net_queue = nullptr;
TaskHandle_t s_net_task = nullptr;

int perform_http_get(const NetWorkerRequest &req)
{
    HTTPClient http;
    http.setConnectTimeout(req.options.connect_timeout_ms);
    http.setTimeout(req.options.timeout_ms);
    if (req.options.disable_redirects)
    {
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    }

    const bool is_https = req.url.startsWith("https://");
    bool begin_ok = false;
    WiFiClientSecure secure_client;
    if (is_https)
    {
        if (req.options.insecure_tls)
        {
            secure_client.setInsecure();
        }
        begin_ok = http.begin(secure_client, req.url);
    }
    else
    {
        begin_ok = http.begin(req.url);
    }

    if (!begin_ok)
    {
        return -1;
    }

    const int code = http.GET();
    if (req.response_body)
    {
        if (code > 0)
        {
            *req.response_body = http.getString();
        }
        else
        {
            req.response_body->clear();
        }
    }
    http.end();
    return code;
}

void net_worker_task(void *pv_parameters)
{
    (void)pv_parameters;
    for (;;)
    {
        NetWorkerRequest *req = nullptr;
        if (xQueueReceive(s_net_queue, &req, portMAX_DELAY) != pdPASS || !req)
        {
            continue;
        }
        req->http_code = perform_http_get(*req);
        if (req->done)
        {
            xSemaphoreGive(req->done);
        }
    }
}
} // namespace

bool net_worker_begin(UBaseType_t task_priority, BaseType_t core_id)
{
    if (s_net_task && s_net_queue)
    {
        return true;
    }

    if (!s_net_queue)
    {
        s_net_queue = xQueueCreate(6, sizeof(NetWorkerRequest *));
        if (!s_net_queue)
        {
            Serial.println("[net] queue create failed");
            return false;
        }
    }

    if (!s_net_task)
    {
        const BaseType_t ok = xTaskCreatePinnedToCore(
            net_worker_task,
            "net_worker",
            6144,
            nullptr,
            task_priority,
            &s_net_task,
            core_id);
        if (ok != pdPASS)
        {
            s_net_task = nullptr;
            Serial.println("[net] task start failed");
            return false;
        }
    }

    Serial.println("[net] worker ready");
    return true;
}

bool net_worker_is_ready()
{
    return s_net_queue != nullptr && s_net_task != nullptr;
}

int net_worker_http_get(const String &url, String *response_body, const NetWorkerHttpOptions *options, uint32_t wait_timeout_ms)
{
    NetWorkerRequest req;
    req.url = url;
    if (options)
    {
        req.options = *options;
    }
    req.response_body = response_body;
    req.done = xSemaphoreCreateBinary();
    if (!req.done)
    {
        return -1;
    }

    int code = -1;
    if (net_worker_is_ready())
    {
        NetWorkerRequest *req_ptr = &req;
        if (xQueueSend(s_net_queue, &req_ptr, pdMS_TO_TICKS(1000)) == pdPASS)
        {
            if (xSemaphoreTake(req.done, pdMS_TO_TICKS(wait_timeout_ms)) == pdPASS)
            {
                code = req.http_code;
            }
        }
    }
    else
    {
        code = perform_http_get(req);
    }

    vSemaphoreDelete(req.done);
    return code;
}
