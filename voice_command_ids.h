#pragma once

#include <Arduino.h>

enum VoiceCommandId : uint16_t {
    VOICE_COMMAND_NONE = 0,
    VOICE_COMMAND_GO_HOME = 1,
    VOICE_COMMAND_OPEN_RADIO = 2,
    VOICE_COMMAND_OPEN_SETTINGS = 3,
    VOICE_COMMAND_OPEN_ASSISTANT = 4,
    VOICE_COMMAND_OPEN_CLOCK = 5,
    VOICE_COMMAND_NEXT_SCREEN = 6,
    VOICE_COMMAND_PREVIOUS_SCREEN = 7,
    VOICE_COMMAND_PLAY_HITS = 8,
    VOICE_COMMAND_PLAY_LOUNGE = 9,
    VOICE_COMMAND_PLAY_PORTUGAL = 10,
    VOICE_COMMAND_PLAY_INDIE = VOICE_COMMAND_PLAY_PORTUGAL,
    VOICE_COMMAND_STOP_RADIO = 11,
    VOICE_COMMAND_VOLUME_UP = 12,
    VOICE_COMMAND_VOLUME_DOWN = 13,
    VOICE_COMMAND_BRIGHTNESS_UP = 14,
    VOICE_COMMAND_BRIGHTNESS_DOWN = 15,
    VOICE_COMMAND_SELF = 16,
    VOICE_COMMAND_ASK = 17,
    VOICE_COMMAND_MURMUR = 18,
    VOICE_COMMAND_MIC_ON = 19,
    VOICE_COMMAND_MIC_OFF = 20,
    VOICE_COMMAND_ALARM_ON = 21,
    VOICE_COMMAND_ALARM_OFF = 22,
};

static inline const char *voice_command_name(uint16_t command_id) {
    switch (command_id) {
        case VOICE_COMMAND_GO_HOME: return "go home";
        case VOICE_COMMAND_OPEN_RADIO: return "open radio";
        case VOICE_COMMAND_OPEN_SETTINGS: return "open settings";
        case VOICE_COMMAND_OPEN_ASSISTANT: return "open assistant";
        case VOICE_COMMAND_OPEN_CLOCK: return "open clock";
        case VOICE_COMMAND_NEXT_SCREEN: return "next screen";
        case VOICE_COMMAND_PREVIOUS_SCREEN: return "previous screen";
        case VOICE_COMMAND_PLAY_HITS: return "play hits";
        case VOICE_COMMAND_PLAY_LOUNGE: return "play lounge";
        case VOICE_COMMAND_PLAY_PORTUGAL: return "play portugal";
        case VOICE_COMMAND_STOP_RADIO: return "stop radio";
        case VOICE_COMMAND_VOLUME_UP: return "volume up";
        case VOICE_COMMAND_VOLUME_DOWN: return "volume down";
        case VOICE_COMMAND_BRIGHTNESS_UP: return "brightness up";
        case VOICE_COMMAND_BRIGHTNESS_DOWN: return "brightness down";
        case VOICE_COMMAND_SELF: return "self";
        case VOICE_COMMAND_ASK: return "ask";
        case VOICE_COMMAND_MURMUR: return "murmur";
        case VOICE_COMMAND_MIC_ON: return "mic on";
        case VOICE_COMMAND_MIC_OFF: return "mic off";
        case VOICE_COMMAND_ALARM_ON: return "alarm on";
        case VOICE_COMMAND_ALARM_OFF: return "alarm off";
        default: return "none";
    }
}




