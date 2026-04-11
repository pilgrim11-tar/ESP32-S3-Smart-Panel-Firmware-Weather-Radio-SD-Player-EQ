#pragma once

#include <stddef.h>

bool stt_cloud_configured();
const char *stt_cloud_language();
void stt_cloud_set_language_override(const char *language_code);
const char *stt_cloud_last_error();
bool stt_cloud_transcribe_sd_wav(const char *wav_path, char *out_text, size_t out_size);

bool tts_cloud_configured();
const char *tts_cloud_model();
const char *tts_cloud_last_error();
bool tts_cloud_synthesize_to_sd_mp3(const char *text, const char *out_path);
