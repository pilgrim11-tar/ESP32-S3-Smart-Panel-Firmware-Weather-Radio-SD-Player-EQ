#pragma once

#include <Arduino.h>

#include "voice_command_ids.h"

uint16_t voice_command_match_transcript(const char *transcript, const char *language_code);
