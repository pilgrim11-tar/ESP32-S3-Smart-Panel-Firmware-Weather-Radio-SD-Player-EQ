#include "mic_remote_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>

namespace {

constexpr uint16_t kMicPort = 8080;
constexpr uint32_t kPollIntervalMs = 450;
constexpr uint32_t kOfflineIntervalMs = 1500;
constexpr uint8_t kOfflineFailThreshold = 24;
constexpr uint32_t kOfflineGraceMs = 12000;
constexpr int kStatusConnectTimeoutMs = 180;
constexpr int kStatusReadTimeoutMs = 220;
constexpr int kQuickRequestTimeoutMs = 140;
constexpr size_t kStatusJsonDocSize = 1024;
const IPAddress kMicDonorIp(192, 168, 50, 2);

bool s_online = false;
bool s_ready = false;
bool s_voice = false;
uint32_t s_rms = 0;
uint32_t s_rms_smooth = 0;
uint32_t s_noise_floor = 0;
uint32_t s_gate_on = 0;
uint32_t s_peak = 0;
bool s_listen_active = false;
bool s_recording = false;
bool s_clip_ready = false;
bool s_radio_playing = false;
int s_radio_index = -1;
String s_listen_phase = "idle";
uint32_t s_lastPollMs = 0;
uint32_t s_lastGoodStatusMs = 0;
bool s_changed = false;
String s_status = "Mic donor offline";
uint8_t s_fail_count = 0;
bool s_transfer_busy = false;
bool s_poll_suspended = false;
String s_visual_state = "idle";
uint32_t s_visual_total_ms = 0;
uint32_t s_visual_remaining_ms = 0;
uint32_t s_last_vu_push_ms = 0;
bool s_last_vu_active = false;
uint32_t s_vu_retry_after_ms = 0;
uint16_t s_pending_command_id = 0;
uint32_t s_pending_command_seq = 0;
uint32_t s_last_command_seq = 0;

void setOffline(const char *reason) {
  const bool old_online = s_online;
  const String old_status = s_status;
  s_online = false;
  s_ready = false;
  s_voice = false;
  s_rms = 0;
  s_rms_smooth = 0;
  s_noise_floor = 0;
  s_gate_on = 0;
  s_peak = 0;
  s_listen_active = false;
  s_recording = false;
  s_clip_ready = false;
  s_radio_playing = false;
  s_radio_index = -1;
  s_listen_phase = "idle";
  s_status = reason;
  s_changed = s_changed || old_online || old_status != s_status;
}

String buildUrl(const char *path) {
  return "http://" + kMicDonorIp.toString() + ":" + String(kMicPort) + path;
}

bool should_mark_offline() {
  s_fail_count = (uint8_t)min<int>(s_fail_count + 1, 255);
  if (s_lastGoodStatusMs == 0) {
    return s_fail_count >= kOfflineFailThreshold;
  }
  return s_fail_count >= 4 && (millis() - s_lastGoodStatusMs) >= kOfflineGraceMs;
}

bool performGetOk(const String &url, int timeoutMs = kQuickRequestTimeoutMs) {
  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  if (!http.begin(url)) {
    return false;
  }
  const int code = http.GET();
  http.end();
  return code == HTTP_CODE_OK;
}

bool ackCommandSeq(uint32_t seq) {
  if (seq == 0) {
    return false;
  }
  const String path = "/command/ack?seq=" + String(seq);
  return performGetOk(buildUrl(path.c_str()), 800);
}

bool sendVisualState(const char *mode, uint32_t remainingMs = 0, uint32_t totalMs = 0) {
  if (!s_online || s_transfer_busy || s_poll_suspended) {
    return false;
  }
  String path = "/ui/state?mode=" + String(mode ? mode : "idle");
  if (remainingMs > 0) {
    path += "&remaining_ms=" + String(remainingMs);
  }
  if (totalMs > 0) {
    path += "&total_ms=" + String(totalMs);
  }
  const bool ok = performGetOk(buildUrl(path.c_str()), 800);
  if (ok) {
    s_visual_state = mode ? mode : "idle";
    s_visual_remaining_ms = remainingMs;
    s_visual_total_ms = totalMs;
  }
  return ok;
}

void fetchStatus() {
  if (s_transfer_busy || s_poll_suspended) {
    return;
  }

  HTTPClient http;
  String url = buildUrl("/status");
  http.setConnectTimeout(kStatusConnectTimeoutMs);
  http.setTimeout(kStatusReadTimeoutMs);
  if (!http.begin(url)) {
    if (should_mark_offline()) {
      setOffline("Mic donor begin failed");
    }
    return;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    if (should_mark_offline()) {
      setOffline("Mic donor HTTP fail");
    }
    return;
  }

  DynamicJsonDocument doc(kStatusJsonDocSize);
  const DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err != DeserializationError::Ok) {
    if (should_mark_offline()) {
      setOffline("Mic donor JSON fail");
    }
    return;
  }

  s_fail_count = 0;
  s_lastGoodStatusMs = millis();

  const bool next_online = doc["ok"] | false;
  const bool next_ready = doc["ready"] | false;
  const bool next_voice = doc["voice"] | false;
  const uint32_t next_rms = doc["rms"] | 0U;
  const uint32_t next_rms_smooth = doc["rms_smooth"] | next_rms;
  const uint32_t next_noise_floor = doc["noise_floor"] | 0U;
  const uint32_t next_gate_on = doc["gate_on"] | 0U;
  const uint32_t next_peak = doc["peak"] | 0U;
  const bool next_listen_active = doc["listen_active"] | false;
  const bool next_recording = doc["recording"] | false;
  const bool next_clip_ready = doc["clip_ready"] | false;
  const bool next_radio_playing = doc["radio_playing"] | false;
  const int next_radio_index = doc["radio_index"] | -1;
  const String next_listen_phase = String((const char *)(doc["listen_phase"] | "idle"));
  const uint16_t next_command_id = doc["command_id"] | 0U;
  const uint32_t next_command_seq = doc["command_seq"] | 0U;

  String next_status = next_online ? "Mic donor online" : "Mic donor offline";
  if (next_online) {
    next_status += next_voice ? " | voice" : " | idle";
    if (next_clip_ready) {
      next_status += " | clip";
    } else if (next_listen_active) {
      next_status += next_recording ? " | rec" : " | wait";
    }
  }

  const bool changed =
      (s_status != next_status) ||
      (s_online != next_online) ||
      (s_ready != next_ready) ||
      (s_voice != next_voice) ||
      (s_listen_active != next_listen_active) ||
      (s_recording != next_recording) ||
      (s_clip_ready != next_clip_ready) ||
      (s_radio_playing != next_radio_playing) ||
      (s_radio_index != next_radio_index) ||
      (s_listen_phase != next_listen_phase);
  s_online = next_online;
  s_ready = next_ready;
  s_voice = next_voice;
  s_rms = next_rms;
  s_rms_smooth = next_rms_smooth;
  s_noise_floor = next_noise_floor;
  s_gate_on = next_gate_on;
  s_peak = next_peak;
  s_listen_active = next_listen_active;
  s_recording = next_recording;
  s_clip_ready = next_clip_ready;
  s_radio_playing = next_radio_playing;
  s_radio_index = next_radio_index;
  s_listen_phase = next_listen_phase;
  s_status = next_status;
  if (next_command_id != 0 && next_command_seq != 0 && next_command_seq != s_last_command_seq) {
    s_pending_command_id = next_command_id;
    s_pending_command_seq = next_command_seq;
    s_last_command_seq = next_command_seq;
    s_changed = true;
  }
  s_changed = s_changed || changed;
}

}  // namespace

void mic_remote_client_init() {
  s_lastGoodStatusMs = 0;
  setOffline("Mic donor offline");
}

void mic_remote_client_loop() {
  if (s_transfer_busy || s_poll_suspended) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t interval = s_online ? kPollIntervalMs : kOfflineIntervalMs;
  if (now - s_lastPollMs < interval) {
    return;
  }
  fetchStatus();
  s_lastPollMs = millis();
}

void mic_remote_client_force_refresh() {
  if (s_transfer_busy || s_poll_suspended) {
    return;
  }
  s_lastPollMs = 0;
  fetchStatus();
}

void mic_remote_client_suspend_polling(bool suspend) {
  s_poll_suspended = suspend;
  if (!suspend) {
    s_lastPollMs = 0;
    fetchStatus();
  }
}

bool mic_remote_client_polling_suspended() {
  return s_poll_suspended;
}

bool mic_remote_client_listen_start(uint32_t wait_ms, uint32_t max_ms, uint32_t silence_ms) {
  if (!s_online) {
    fetchStatus();
    if (!s_online) {
      return false;
    }
  }

  String path = "/listen/start?wait_ms=" + String(wait_ms) +
                "&max_ms=" + String(max_ms) +
                "&silence_ms=" + String(silence_ms);
  const bool ok = performGetOk(buildUrl(path.c_str()));
  if (ok) {
    fetchStatus();
  }
  return ok;
}

bool mic_remote_client_listen_stop() {
  if (!s_online) {
    return false;
  }
  const bool ok = performGetOk(buildUrl("/listen/stop"));
  if (ok) {
    fetchStatus();
  }
  return ok;
}

bool mic_remote_client_download_wav(const char *sd_path) {
  if (!sd_path || !s_online || !s_clip_ready) {
    return false;
  }

  s_transfer_busy = true;

  HTTPClient http;
  http.setConnectTimeout(1800);
  http.setTimeout(12000);
  if (!http.begin(buildUrl("/listen/wav"))) {
    s_transfer_busy = false;
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    s_transfer_busy = false;
    return false;
  }

  SD.remove(sd_path);
  File out = SD.open(sd_path, FILE_WRITE);
  if (!out) {
    http.end();
    s_transfer_busy = false;
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[1024];
  int remaining = http.getSize();
  size_t written = 0;
  while (http.connected() && (remaining > 0 || remaining == -1)) {
    const size_t available = stream->available();
    if (!available) {
      delay(1);
      continue;
    }
    const int read = stream->readBytes(buffer, min<size_t>(available, sizeof(buffer)));
    if (read <= 0) {
      break;
    }
    out.write(buffer, read);
    written += static_cast<size_t>(read);
    if (remaining > 0) {
      remaining -= read;
    }
  }

  out.close();
  http.end();
  s_transfer_busy = false;
  fetchStatus();
  return written > 44;
}

bool mic_remote_client_online() {
  return s_online;
}

bool mic_remote_client_ready() {
  return s_online && s_ready;
}

bool mic_remote_client_voice() {
  return s_online && s_voice;
}

bool mic_remote_client_listen_active() {
  return s_online && s_listen_active;
}

bool mic_remote_client_recording() {
  return s_online && s_recording;
}

bool mic_remote_client_clip_ready() {
  return s_online && s_clip_ready;
}

uint16_t mic_remote_client_consume_command() {
  const uint16_t command_id = s_pending_command_id;
  const uint32_t command_seq = s_pending_command_seq;
  s_pending_command_id = 0;
  s_pending_command_seq = 0;
  if (command_id != 0) {
    ackCommandSeq(command_seq);
  }
  return command_id;
}

uint32_t mic_remote_client_rms() {
  return s_rms;
}

uint32_t mic_remote_client_rms_smooth() {
  return s_rms_smooth;
}

uint32_t mic_remote_client_noise_floor() {
  return s_noise_floor;
}

uint32_t mic_remote_client_gate_on() {
  return s_gate_on;
}

uint32_t mic_remote_client_peak() {
  return s_peak;
}

const char *mic_remote_client_status() {
  return s_status.c_str();
}

const char *mic_remote_client_listen_phase() {
  return s_listen_phase.c_str();
}

bool mic_remote_client_consume_changed() {
  const bool changed = s_changed;
  s_changed = false;
  return changed;
}

void mic_remote_client_set_visual_idle() {
  sendVisualState("idle");
}

void mic_remote_client_set_visual_armed() {
  sendVisualState("armed");
}

void mic_remote_client_set_visual_cooldown(uint32_t remaining_ms, uint32_t total_ms) {
  if (total_ms == 0) {
    total_ms = remaining_ms;
  }
  sendVisualState("cooldown", remaining_ms, total_ms);
}

void mic_remote_client_set_visual_disabled() {
  sendVisualState("disabled");
}

bool mic_remote_client_set_command_language(const char *code) {
  String lang = code ? code : "en";
  lang.trim();
  lang.toLowerCase();
  if (lang.length() == 0) {
    lang = "en";
  }
  String path = "/command/lang?code=" + lang;
  return performGetOk(buildUrl(path.c_str()), 800);
}

bool mic_remote_client_bt_start() {
  return performGetOk(buildUrl("/bt/start"), 800);
}

bool mic_remote_client_bt_stop() {
  return performGetOk(buildUrl("/bt/stop"), 800);
}

bool mic_remote_client_radio_start_index(int idx) {
  String path = "/radio/start?idx=" + String(max(0, idx));
  const bool ok = performGetOk(buildUrl(path.c_str()), 260);
  if (ok) {
    s_radio_playing = true;
    s_radio_index = max(0, idx);
  }
  return ok;
}

bool mic_remote_client_radio_next() {
  const bool ok = performGetOk(buildUrl("/radio/next"), 260);
  return ok;
}

bool mic_remote_client_radio_stop() {
  const bool ok = performGetOk(buildUrl("/radio/stop"), 240);
  if (ok) {
    s_radio_playing = false;
    s_radio_index = -1;
  }
  return ok;
}

bool mic_remote_client_radio_playing() {
  return s_online && s_radio_playing;
}

int mic_remote_client_radio_index() {
  return s_radio_index;
}

void mic_remote_client_push_vu(const uint8_t *levels, size_t count, bool active) {
  const uint32_t now = millis();
  static uint32_t s_vu_probe_after_ms = 0;
  if (!levels || count == 0 || s_transfer_busy || s_poll_suspended) {
    return;
  }
  if (!s_online) {
    if (!active || now < s_vu_probe_after_ms) {
      return;
    }
    // Donor can be reachable for /vu before status poll marks it online.
    s_vu_probe_after_ms = now + 1500;
  }
  if (now < s_vu_retry_after_ms) {
    return;
  }
  if ((now - s_last_vu_push_ms) < 320 && active == s_last_vu_active) {
    return;
  }

  String path = "/vu?active=";
  path += active ? "1" : "0";
  path += "&bars=";
  const size_t n = min<size_t>(count, 8);
  for (size_t i = 0; i < n; ++i) {
    if (i) {
      path += ',';
    }
    path += String(levels[i]);
  }

  if (performGetOk(buildUrl(path.c_str()), 70)) {
    s_last_vu_push_ms = now;
    s_last_vu_active = active;
    s_vu_retry_after_ms = 0;
    if (!s_online) {
      // Keep donor state warm without waiting for next /status poll.
      s_online = true;
    }
  } else {
    s_vu_retry_after_ms = now + 1200;
  }
}





