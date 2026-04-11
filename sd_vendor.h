#pragma once

#include <Arduino.h>

enum SdVoiceMode
{
    SD_VOICE_DAY = 0,
    SD_VOICE_EVENING,
    SD_VOICE_NIGHT,
};

bool sd_vendor_init();
bool sd_vendor_ensure_ready();
bool sd_vendor_ready();
const char *sd_vendor_status();

bool sd_vendor_is_busy();
bool sd_vendor_play_random(const char *folder);
bool sd_vendor_music_refresh();
bool sd_vendor_music_loaded();
bool sd_vendor_music_play_current();
bool sd_vendor_music_play_next();
bool sd_vendor_music_play_previous();
bool sd_vendor_music_play_index(int index);
bool sd_vendor_music_play_active_position(int position);
bool sd_vendor_music_play_random_current_album();
int sd_vendor_music_count();
int sd_vendor_music_index();
const char *sd_vendor_music_title();
const char *sd_vendor_music_title_at(int index);
const char *sd_vendor_music_title_at_active(int position);
const char *sd_vendor_music_group_at_active(int position);
const char *sd_vendor_music_path_at_active(int position);
const char *sd_vendor_music_group();
const char *sd_vendor_music_path();
int sd_vendor_music_album_count();
int sd_vendor_music_album_index();
const char *sd_vendor_music_album_name();
const char *sd_vendor_music_album_name_at(int index);
bool sd_vendor_music_select_album(int index);
bool sd_vendor_music_select_album_next(int delta);
bool sd_vendor_music_album_has_children(int index);
bool sd_vendor_music_enter_selected_album();
bool sd_vendor_music_browser_go_root();
int sd_vendor_music_count_active();
int sd_vendor_music_position_active();
const char *sd_vendor_music_title_at_active_offset(int offset);
bool sd_vendor_play_murmur();
bool sd_vendor_play_selftalk();
bool sd_vendor_play_question();
bool sd_vendor_play_heard();
bool sd_vendor_play_ack();
bool sd_vendor_play_curious();

bool sd_vendor_play_murmur_mode(SdVoiceMode mode);
bool sd_vendor_play_selftalk_mode(SdVoiceMode mode);
bool sd_vendor_play_question_mode(SdVoiceMode mode);
bool sd_vendor_play_heard_mode(SdVoiceMode mode);
bool sd_vendor_play_ack_mode(SdVoiceMode mode);
bool sd_vendor_play_listening_mode(SdVoiceMode mode);
bool sd_vendor_play_curious_mode(SdVoiceMode mode);


