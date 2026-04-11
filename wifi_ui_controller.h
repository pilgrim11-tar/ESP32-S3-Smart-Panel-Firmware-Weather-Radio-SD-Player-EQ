#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>

const char *wifi_ui_status_scanning();
const char *wifi_ui_status_after_scan(int network_count);
const char *wifi_ui_status_connecting();
const char *wifi_ui_status_connect_success();
const char *wifi_ui_status_connect_failed();
const char *wifi_ui_status_connect_task_failed();
String wifi_ui_networks_to_dropdown_options(int network_count);
String wifi_ui_current_connection_text(wl_status_t wifi_status, const String &ssid);
void wifi_ui_textarea_focus_cb(lv_event_t *e);
void wifi_ui_apply_saved_city_text(lv_obj_t *city_textarea, const String &city_query);
