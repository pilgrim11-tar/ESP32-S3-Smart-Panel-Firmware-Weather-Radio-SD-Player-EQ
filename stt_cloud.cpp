#include "stt_cloud.h"

#include <Arduino.h>

namespace {
char s_stt_last_error[64] = "STT disabled";
char s_tts_last_error[64] = "TTS disabled";
char s_language[16] = "off";
}

bool stt_cloud_configured() {
  return false;
}

const char *stt_cloud_language() {
  return s_language;
}

void stt_cloud_set_language_override(const char *language_code) {
  snprintf(s_language, sizeof(s_language), "%s", (language_code && language_code[0]) ? language_code : "off");
}

const char *stt_cloud_last_error() {
  return s_stt_last_error;
}

bool stt_cloud_transcribe_sd_wav(const char *wav_path, char *out_text, size_t out_size) {
  (void)wav_path;
  if (out_text && out_size > 0) {
    out_text[0] = '\0';
  }
  snprintf(s_stt_last_error, sizeof(s_stt_last_error), "%s", "STT disabled");
  return false;
}

bool tts_cloud_configured() {
  return false;
}

const char *tts_cloud_model() {
  return "disabled";
}

const char *tts_cloud_last_error() {
  return s_tts_last_error;
}

bool tts_cloud_synthesize_to_sd_mp3(const char *text, const char *out_path) {
  (void)text;
  (void)out_path;
  snprintf(s_tts_last_error, sizeof(s_tts_last_error), "%s", "TTS disabled");
  return false;
}
