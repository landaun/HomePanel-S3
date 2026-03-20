#include "command_queue.h"
#include "ha_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

static const char* TAG = "CMDQ";
static QueueHandle_t cmd_queue;

static void command_queue_coalesce(ha_cmd_t* cmd)
{
    if (!cmd_queue || !cmd)
    {
        return;
    }

    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(COMMAND_QUEUE_COALESCE_MS);
    ha_cmd_t next;

    while (xTaskGetTickCount() < deadline)
    {
        if (xQueuePeek(cmd_queue, &next, pdMS_TO_TICKS(COMMAND_QUEUE_COALESCE_MS)) != pdTRUE)
        {
            break;
        }
        if (next.type == cmd->type && strcmp(next.entity_id, cmd->entity_id) == 0)
        {
            xQueueReceive(cmd_queue, &next, 0);
            if (cmd->data)
            {
                cJSON_Delete(cmd->data);
            }
            *cmd = next;
            deadline = xTaskGetTickCount() + pdMS_TO_TICKS(COMMAND_QUEUE_COALESCE_MS);
        }
        else
        {
            break;
        }
    }
}

static void command_worker(void* arg)
{
    ha_cmd_t cmd;
    ESP_LOGI(TAG, "Command worker task started");
    while (xQueueReceive(cmd_queue, &cmd, portMAX_DELAY))
    {
        ESP_LOGI(TAG, "=== COMMAND WORKER: Received command type=%d ===", cmd.type);
        command_queue_coalesce(&cmd);
        esp_err_t err = ESP_OK;
        switch (cmd.type)
        {
        case HA_CMD_SERVICE:
            ESP_LOGI(TAG, "Processing HA_CMD_SERVICE: domain=%s, service=%s", cmd.domain, cmd.service);
            err = ha_client_call_service(cmd.domain, cmd.service, cmd.data);
            break;
        case HA_CMD_LIGHT:
            ESP_LOGI(TAG, "Processing HA_CMD_LIGHT (turn_on): entity=%s", cmd.entity_id);
            err = ha_client_light_turn_on(cmd.entity_id, cmd.data);
            break;
        case HA_CMD_LIGHT_OFF:
            ESP_LOGI(TAG, "Processing HA_CMD_LIGHT_OFF (turn_off): entity=%s", cmd.entity_id);
            err = ha_client_light_turn_off(cmd.entity_id);
            break;
        case HA_CMD_SCENE:
            ESP_LOGI(TAG, "Processing HA_CMD_SCENE: entity=%s", cmd.entity_id);
            err = ha_client_scene_turn_on(cmd.entity_id);
            break;
        case HA_CMD_AUTOMATION:
            ESP_LOGI(TAG, "Processing HA_CMD_AUTOMATION: entity=%s", cmd.entity_id);
            err = ha_client_automation_trigger(cmd.entity_id);
            break;
        case HA_CMD_REQUEST:
            ESP_LOGI(TAG, "Processing HA_CMD_REQUEST: method=%s, path=%s", cmd.method, cmd.path);
            err = ha_client_request(cmd.method, cmd.path, cmd.data, NULL);
            break;
        default:
            ESP_LOGE(TAG, "Unknown command type: %d", cmd.type);
            break;
        }
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Command failed with error: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(TAG, "Command completed successfully");
        }
        if (cmd.data)
        {
            cJSON_Delete(cmd.data);
        }
        ESP_LOGI(TAG, "=== COMMAND WORKER: Command processing complete ===");
    }
}

bool command_queue_init(void)
{
    cmd_queue = xQueueCreate(COMMAND_QUEUE_LEN, sizeof(ha_cmd_t));
    if (!cmd_queue)
    {
        return false;
    }
#ifndef UNIT_TEST
    TaskHandle_t task_handle = NULL;
    BaseType_t result = xTaskCreate(command_worker, "cmd_worker", 4096, NULL, 5, &task_handle);
    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create command worker task");
        vQueueDelete(cmd_queue);
        cmd_queue = NULL;
        return false;
    }
    ESP_LOGI(TAG, "Command worker task created successfully");
#endif
    return true;
}

bool command_queue_receive(ha_cmd_t* out)
{
    if (!cmd_queue || !out)
    {
        return false;
    }
    return xQueueReceive(cmd_queue, out, 0) == pdTRUE;
}

bool command_queue_enqueue(const ha_cmd_t* cmd)
{
    ESP_LOGI(TAG, ">>> command_queue_enqueue: cmd=%p, cmd_queue=%p", cmd, cmd_queue);

    if (!cmd_queue)
    {
        ESP_LOGE(TAG, "<<< command_queue_enqueue: FAILED - cmd_queue is NULL (not initialized)");
        return false;
    }

    if (!cmd)
    {
        ESP_LOGE(TAG, "<<< command_queue_enqueue: FAILED - cmd is NULL");
        return false;
    }

    ESP_LOGI(TAG, "Enqueueing command type %d", cmd->type);

    BaseType_t result = xQueueSend(cmd_queue, cmd, 0);

    if (result == pdTRUE)
    {
        ESP_LOGI(TAG, "<<< command_queue_enqueue: SUCCESS");
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "<<< command_queue_enqueue: FAILED - xQueueSend returned %d (queue full?)", result);
        ESP_LOGE(TAG, "    Queue spaces available: %d", (int)uxQueueSpacesAvailable(cmd_queue));
        return false;
    }
}

bool command_queue_enqueue_light(const char* entity_id, const cJSON* params)
{
    ESP_LOGI(TAG, ">>> command_queue_enqueue_light: entity='%s', params=%p",
             entity_id ? entity_id : "NULL", params);

    if (!entity_id)
    {
        ESP_LOGE(TAG, "<<< command_queue_enqueue_light: NULL entity_id");
        return false;
    }
    ha_cmd_t cmd = {
        .type = HA_CMD_LIGHT,
        .data = params ? cJSON_Duplicate(params, 1) : NULL,
    };
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    cmd.entity_id[sizeof(cmd.entity_id) - 1] = '\0';

    ESP_LOGI(TAG, "Calling command_queue_enqueue with cmd.type=%d, cmd.entity_id='%s'",
             cmd.type, cmd.entity_id);

    bool result = command_queue_enqueue(&cmd);
    ESP_LOGI(TAG, "<<< command_queue_enqueue_light result: %s", result ? "true" : "false");
    return result;
}

bool command_queue_enqueue_light_raw(const char* entity_id, cJSON* params)
{
    if (!entity_id)
    {
        if (params)
        {
            cJSON_Delete(params);
        }
        return false;
    }
    ha_cmd_t cmd = {
        .type = HA_CMD_LIGHT,
        .data = params,
    };
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    cmd.entity_id[sizeof(cmd.entity_id) - 1] = '\0';
    return command_queue_enqueue(&cmd);
}

bool command_queue_enqueue_light_brightness(const char* entity_id, uint8_t brightness)
{
    cJSON* params = cJSON_CreateObject();
    cJSON_AddNumberToObject(params, "brightness", brightness);
    bool ok = command_queue_enqueue_light(entity_id, params);
    cJSON_Delete(params);
    return ok;
}

bool command_queue_enqueue_light_level(const char* entity_id, uint8_t level)
{
    return command_queue_enqueue_light_brightness(entity_id, level);
}

bool command_queue_enqueue_light_color_temp(const char* entity_id, uint16_t mireds)
{
    cJSON* params = cJSON_CreateObject();
    cJSON_AddNumberToObject(params, "color_temp", mireds);
    bool ok = command_queue_enqueue_light(entity_id, params);
    cJSON_Delete(params);
    return ok;
}

bool command_queue_enqueue_light_rgb(const char* entity_id, uint8_t r, uint8_t g, uint8_t b)
{
    cJSON* params = cJSON_CreateObject();
    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(r));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(g));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(b));
    cJSON_AddItemToObject(params, "rgb_color", arr);
    bool ok = command_queue_enqueue_light(entity_id, params);
    cJSON_Delete(params);
    return ok;
}

bool command_queue_enqueue_light_hs(const char* entity_id, float h, float s)
{
    cJSON* params = cJSON_CreateObject();
    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(h));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(s));
    cJSON_AddItemToObject(params, "hs_color", arr);
    bool ok = command_queue_enqueue_light(entity_id, params);
    cJSON_Delete(params);
    return ok;
}

bool command_queue_enqueue_light_effect(const char* entity_id, const char* effect)
{
    if (!effect)
    {
        return false;
    }
    cJSON* params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "effect", effect);
    bool ok = command_queue_enqueue_light(entity_id, params);
    cJSON_Delete(params);
    return ok;
}

bool command_queue_enqueue_light_on(const char* entity_id)
{
    ESP_LOGI(TAG, ">>> command_queue_enqueue_light_on: entity='%s'", entity_id ? entity_id : "NULL");
    bool result = command_queue_enqueue_light(entity_id, NULL);
    ESP_LOGI(TAG, "<<< command_queue_enqueue_light_on result: %s", result ? "true" : "false");
    return result;
}

bool command_queue_enqueue_light_off(const char* entity_id)
{
    ESP_LOGI(TAG, ">>> command_queue_enqueue_light_off: entity='%s'", entity_id ? entity_id : "NULL");
    if (!entity_id)
    {
        ESP_LOGE(TAG, "<<< command_queue_enqueue_light_off: NULL entity_id");
        return false;
    }
    ha_cmd_t cmd = {
        .type = HA_CMD_LIGHT_OFF,
        .data = NULL,
    };
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    cmd.entity_id[sizeof(cmd.entity_id) - 1] = '\0';
    bool result = command_queue_enqueue(&cmd);
    ESP_LOGI(TAG, "<<< command_queue_enqueue_light_off result: %s", result ? "true" : "false");
    return result;
}

bool command_queue_enqueue_light_toggle(const char* entity_id, bool on)
{
    return on ? command_queue_enqueue_light_on(entity_id)
              : command_queue_enqueue_light_off(entity_id);
}

bool command_queue_enqueue_scene(const char* entity_id)
{
    if (!entity_id)
    {
        return false;
    }
    ha_cmd_t cmd = {
        .type = HA_CMD_SCENE,
        .data = NULL,
    };
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    cmd.entity_id[sizeof(cmd.entity_id) - 1] = '\0';
    return command_queue_enqueue(&cmd);
}

bool command_queue_enqueue_automation(const char* entity_id)
{
    if (!entity_id)
    {
        return false;
    }
    ha_cmd_t cmd = {
        .type = HA_CMD_AUTOMATION,
        .data = NULL,
    };
    strncpy(cmd.entity_id, entity_id, sizeof(cmd.entity_id) - 1);
    cmd.entity_id[sizeof(cmd.entity_id) - 1] = '\0';
    return command_queue_enqueue(&cmd);
}

bool command_queue_enqueue_service(const char* domain, const char* service, const cJSON* data)
{
    if (!domain || !service)
    {
        return false;
    }
    ha_cmd_t cmd = {
        .type = HA_CMD_SERVICE,
        .data = data ? cJSON_Duplicate(data, 1) : NULL,
    };
    strncpy(cmd.domain, domain, sizeof(cmd.domain) - 1);
    cmd.domain[sizeof(cmd.domain) - 1] = '\0';
    strncpy(cmd.service, service, sizeof(cmd.service) - 1);
    cmd.service[sizeof(cmd.service) - 1] = '\0';
    return command_queue_enqueue(&cmd);
}

bool command_queue_enqueue_service_domain(ha_domain_t domain, const char* service,
                                          const cJSON* data)
{
    const char* domain_str = NULL;
    switch (domain)
    {
    case HA_DOMAIN_LIGHT:
        domain_str = HA_DOMAIN_LIGHT_STR;
        break;
    case HA_DOMAIN_SCENE:
        domain_str = HA_DOMAIN_SCENE_STR;
        break;
    case HA_DOMAIN_AUTOMATION:
        domain_str = HA_DOMAIN_AUTOMATION_STR;
        break;
    default:
        return false;
    }
    return command_queue_enqueue_service(domain_str, service, data);
}

bool command_queue_enqueue_service_path(const char* path, const cJSON* data)
{
    return command_queue_enqueue_request("POST", path, data);
}

bool command_queue_enqueue_post(const char* path, const cJSON* data)
{
    return command_queue_enqueue_request("POST", path, data);
}

bool command_queue_enqueue_put(const char* path, const cJSON* data)
{
    return command_queue_enqueue_request("PUT", path, data);
}

bool command_queue_enqueue_request(const char* method, const char* path, const cJSON* data)
{
    if (!method || !path)
    {
        return false;
    }
    ha_cmd_t cmd = {
        .type = HA_CMD_REQUEST,
        .data = data ? cJSON_Duplicate(data, 1) : NULL,
    };
    strncpy(cmd.method, method, sizeof(cmd.method) - 1);
    cmd.method[sizeof(cmd.method) - 1] = '\0';
    strncpy(cmd.path, path, sizeof(cmd.path) - 1);
    cmd.path[sizeof(cmd.path) - 1] = '\0';
    return command_queue_enqueue(&cmd);
}

bool command_queue_enqueue_get_state(const char* entity_id)
{
    if (!entity_id)
    {
        return false;
    }
    char path[HA_MAX_ENTITY_ID_LEN + 32];
    snprintf(path, sizeof(path), HA_API_STATE_FMT, entity_id);
    return command_queue_enqueue_request("GET", path, NULL);
}

bool command_queue_enqueue_get_states(void)
{
    return command_queue_enqueue_request("GET", HA_API_STATES, NULL);
}

size_t command_queue_pending(void)
{
    if (!cmd_queue)
    {
        return 0;
    }
    return (size_t)uxQueueMessagesWaiting(cmd_queue);
}

bool command_queue_is_empty(void)
{
    if (!cmd_queue)
    {
        return true;
    }
    return uxQueueMessagesWaiting(cmd_queue) == 0;
}

bool command_queue_initialized(void) { return cmd_queue != NULL; }

void command_queue_flush(void)
{
    if (cmd_queue)
    {
        xQueueReset(cmd_queue);
    }
}

void command_queue_shutdown(void)
{
    if (cmd_queue)
    {
        vQueueDelete(cmd_queue);
        cmd_queue = NULL;
    }
}
