#include "doMain.h"
#include "audio_visualizer.h"
#include "app_settings.h"
#include "doWifi.h"
#include "flip_clock.h"
#include "home_bg_sd_assets.h"
#include "mic_remote_client.h"
#include "net_worker.h"
#include "power_manager.h"
#include "radio_vendor.h"
#include "sd_vendor.h"
#include "speaker_tone.h"
#include "stt_cloud.h"
#include "voice_command_ids.h"
#include "voice_command_parser.h"
#include "wake_logic.h"
#include "wifi_manager.h"
#include "wifi_ui_controller.h"
#include "weather_sd_assets.h"
#include "weather_visuals.h"
#include "ui_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif
LV_IMG_DECLARE(ui_img_radio_play_on_png);
LV_IMG_DECLARE(ui_img_radio_play_off_png);
LV_IMG_DECLARE(ui_img_radio_stop_on_png);
LV_IMG_DECLARE(ui_img_radio_stop_off_png);
LV_IMG_DECLARE(ui_img_radio_hits_on_png);
LV_IMG_DECLARE(ui_img_radio_hits_off_png);
LV_IMG_DECLARE(ui_img_radio_lounge_on_png);
LV_IMG_DECLARE(ui_img_radio_lounge_off_png);
LV_IMG_DECLARE(ui_img_radio_portugal_on_png);
LV_IMG_DECLARE(ui_img_radio_portugal_off_png);
LV_IMG_DECLARE(ui_img_media_pause_png);
LV_IMG_DECLARE(ui_img_media_back_png);
LV_IMG_DECLARE(ui_img_media_note_png);
LV_IMG_DECLARE(ui_img_media_next_png);
LV_IMG_DECLARE(ui_img_s4_card4_png);
LV_IMG_DECLARE(ui_img_sound_modes_322_png);
LV_IMG_DECLARE(ui_img_settings_volume_png);
LV_IMG_DECLARE(ui_img_settings_brightness_png);
LV_IMG_DECLARE(ui_img_settings_mic_png);
#ifdef __cplusplus
}
#endif

#include <Preferences.h>
#include <SD.h>
#include <img_converters.h>
#include <ctype.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <stdint.h>

// you can in here define the wifi ssid and password and the weather api key and the city position
String ssid = "";
String pswd = "";
String lat = "";
String lon = "";
static String g_city_name = "";
static String g_city_query = "";
static String g_country_name = "";
static String g_country_code = "";
static String g_holiday_text = "";
static float g_weather_saved_lat = 0.0f;
static float g_weather_saved_lon = 0.0f;
static constexpr float kWeatherDefaultLat = 50.4501f;
static constexpr float kWeatherDefaultLon = 30.5234f;

// Media screen (safe add-on)
static lv_obj_t *g_media_screen = nullptr;
static lv_obj_t *g_media_label = nullptr;
static lv_obj_t *g_media_status_label = nullptr;
static lv_obj_t *g_media_album_label = nullptr;
static lv_obj_t *g_media_meta_title_label = nullptr;
static lv_obj_t *g_media_meta_group_label = nullptr;
static lv_obj_t *g_media_next_title_label = nullptr;
static lv_obj_t *g_media_queue_later_label = nullptr;
static lv_obj_t *g_media_queue_more_label_1 = nullptr;
static lv_obj_t *g_media_queue_more_label_2 = nullptr;
static lv_obj_t *g_media_progress_fill = nullptr;
static lv_obj_t *g_media_progress_arc = nullptr;
static lv_obj_t *g_media_track_counter_label = nullptr;
static lv_obj_t *g_media_btn_play = nullptr;
static lv_obj_t *g_media_btn_pause = nullptr;
static lv_obj_t *g_media_btn_stop = nullptr;
static lv_obj_t *g_media_btn_next = nullptr;
static lv_obj_t *g_media_btn_prev = nullptr;
static lv_obj_t *g_media_btn_back = nullptr;
static lv_obj_t *g_media_btn_random = nullptr;
static lv_obj_t *g_media_btn_album_prev = nullptr;
static lv_obj_t *g_media_btn_album_next = nullptr;
static lv_obj_t *g_media_btn_album_home = nullptr;
static lv_obj_t *g_media_btn_album_scan = nullptr;
static lv_obj_t *g_media_btn_enter = nullptr;
static lv_obj_t *g_media_toggle_icon = nullptr;
static lv_obj_t *g_media_cover_img = nullptr;
static lv_obj_t *g_settings_media_btn = nullptr;
static lv_obj_t *g_media_source_radio_btn = nullptr;
static lv_obj_t *g_media_source_sd_btn = nullptr;

struct MediaCoverSlot
{
    lv_img_dsc_t dsc{};
    uint8_t *data = nullptr;
    size_t capacity = 0;
    char folder[144]{};
    bool valid = false;
    bool checked = false;
};
static MediaCoverSlot g_media_cover_slot;
static char g_media_cover_track_path[192] = {0};

enum MediaSourceKind : uint8_t {
    MEDIA_SOURCE_RADIO = 0,
    MEDIA_SOURCE_SD = 1,
};

enum MediaRadioTaskAction : uint8_t {
    MEDIA_RADIO_TASK_NONE = 0,
    MEDIA_RADIO_TASK_PLAY = 1,
    MEDIA_RADIO_TASK_NEXT = 2,
};

enum HomeLaunchTarget : uint8_t {
    HOME_LAUNCH_RADIO = 0,
    HOME_LAUNCH_PLAYER = 1,
    HOME_LAUNCH_ON = 2,
    HOME_LAUNCH_OFF = 3,
};

static MediaSourceKind g_media_selected_source = MEDIA_SOURCE_RADIO;
static TaskHandle_t g_media_radio_task = NULL;
static volatile bool g_media_radio_task_pending = false;
static volatile bool g_media_radio_task_done = false;
static volatile bool g_media_radio_task_result = false;
static volatile uint8_t g_media_radio_task_action = MEDIA_RADIO_TASK_NONE;
static TaskHandle_t g_media_sd_scan_task = NULL;
static volatile bool g_media_sd_scan_task_pending = false;
static volatile bool g_media_sd_scan_task_done = false;
static volatile bool g_media_sd_scan_task_result = false;

enum MediaBrowserLevel : uint8_t
{
    MEDIA_BROWSER_ALBUMS = 0,
    MEDIA_BROWSER_TRACKS = 1,
};
static MediaBrowserLevel g_media_browser_level = MEDIA_BROWSER_ALBUMS;
static int g_media_browser_album_sel = 0; // 0..album_count-1 (real albums only)
static int g_media_browser_track_sel = 0; // 0..active_track_count-1

static void ensure_media_screen();
static void create_settings_media_button();
static void create_home_launch_buttons();
static void update_home_launch_buttons();
static void open_media_screen();
static void home_launch_event(lv_event_t *e);
static void media_btn_event(lv_event_t *e);
static void media_album_event(lv_event_t *e);
static void media_library_event(lv_event_t *e);
static void media_source_event(lv_event_t *e);
static void media_back_event(lv_event_t *e);
static void media_open_event(lv_event_t *e);
static void stop_all_audio_output();
static void update_media_screen_info();
static void media_cover_apply_default();
static bool media_cover_update_for_track(const char *track_path);
static void update_radio_ui(bool playing, int station_index);
static bool radio_uses_remote();
static bool radio_ctrl_start_index(int idx);
static bool radio_ctrl_start_random();
static bool radio_ctrl_next();
static void radio_ctrl_stop();
static bool radio_ctrl_is_playing();
static int radio_ctrl_current_index();
static bool media_sd_playing();
static bool media_radio_playing();
static bool queue_media_radio_action(MediaRadioTaskAction action);
static void media_radio_action_task(void *parameter);
static void service_media_radio_action();
static bool start_media_sd_scan();
static void media_sd_scan_task(void *parameter);
static void service_media_sd_scan_task();
static void media_browser_reset();
static void media_browser_set_albums();
static void media_browser_set_tracks(bool seed_from_current);
static void media_browser_move(int delta);
static void media_browser_enter();
static String media_album_browser_text(int selected, int total);

int wifi_nums = 0;
long currentTime_year;
byte currentTime_mouth, currentTime_day,
    currentTime_hour, currentTime_minute, currentTime_second, currentTime_week;
unsigned long timeNow;
String topTimeText, hourText, minuteText, secondText, yearText, mouthText, dayText, topDateText, weekText;
bool NTPState;
boolean light1_status = false;
boolean light2_status = false;
boolean light3_status = false;
IPAddress dns(114, 114, 114, 114);
static uint8_t g_backlight = 150;
static uint32_t g_last_user_activity_ms = 0;
static uint32_t g_last_sound_activity_ms = 0;
static uint32_t g_display_wake_hold_until_ms = 0;
static bool g_display_sleeping = false;
static bool g_system_soft_off = false;
static bool g_system_resume_mic_enabled = false;
enum VoiceCommandLanguage : uint8_t { VOICE_LANG_EN = 0, VOICE_LANG_PT_PT = 1 };
static VoiceCommandLanguage g_voice_command_language = VOICE_LANG_EN;
static bool g_ai_voice_enabled = false;
static constexpr bool kMicFeatureEnabled = false;
static bool g_sound_mode_self_enabled = false;
static bool g_sound_mode_ask_enabled = false;
static bool g_sound_mode_murmur_enabled = false;
static bool g_mic_ready = false;
static bool g_time_task_started = false;
static bool g_weather_task_started = false;
static TaskHandle_t g_weather_task_handle = NULL;
static constexpr uint32_t kWeatherTaskStackBytes = 8192;
static bool g_weather_ready = false;
static volatile int g_wifi_connect_state = 0; // 0 idle, 1 pending, 2 success, 3 fail
static bool g_clock_dirty = true;
static bool g_weather_dirty = true;
static int g_weather_code = 3;
static bool g_weather_is_day = true;
static String g_weather_summary = "No internet";
static String g_weather_temp_text = "--";
static String g_weather_wind_text = "--";
static String g_weather_humidity_text = "--";
static String g_clock_timezone_text = "Kyiv time - EET";
static uint32_t g_next_ambient_ms = 0;
static Preferences g_prefs;
static lv_timer_t *g_weather_icon_timer = NULL;
static uint8_t g_weather_icon_frame = 0;
static String g_home_bg_applied_key = "";
static bool g_panel_link_ap_started = false;
static constexpr bool kPanelLinkApEnabled = false;
static constexpr int kDonorEqUartTxPin = 41;
static constexpr uint32_t kDonorEqBaud = 115200;
static HardwareSerial gDonorEqSerial(1);
static uint8_t gDonorEqSeq = 0;
static constexpr bool kDonorWakeGpioEnabled = false;
static constexpr int kDonorWakeGpioInPin = 44; // P2 pin 1 (U0RXD)
static constexpr uint32_t kDonorWakeHoldMs = 1500;
static lv_obj_t *g_boot_black_screen = NULL;
static bool g_boot_black_active = false;
static bool g_boot_wait_wifi_on_start = false;

static lv_obj_t *g_home_wifi_label = NULL;
static lv_obj_t *g_home_state_label = NULL;
static lv_obj_t *g_home_hint_label = NULL;
static lv_obj_t *g_home_radio_btn = NULL;
static lv_obj_t *g_home_player_btn = NULL;
static lv_obj_t *g_home_voice_btn = NULL;
static lv_obj_t *g_home_off_btn = NULL;
static lv_obj_t *g_settings_title_label = NULL;
static lv_obj_t *g_settings_voice_label = NULL;
static lv_obj_t *g_settings_voice_value_label = NULL;
static lv_obj_t *g_settings_voice_hitbox = NULL;
static lv_obj_t *g_settings_voice_cover = NULL;
static lv_obj_t *g_settings_row1_label = NULL;
static lv_obj_t *g_settings_row2_label = NULL;
static lv_obj_t *g_settings_volume_icon_label = NULL;
static lv_obj_t *g_settings_mic_icon_img = NULL;
static lv_obj_t *g_radio_title_label = NULL;
static lv_obj_t *g_radio_status_label = NULL;
static lv_obj_t *g_ai_header_icon = NULL;
static lv_obj_t *g_ai_title_label = NULL;
static lv_obj_t *g_ai_status_label = NULL;
static lv_obj_t *g_ai_card_title_1 = NULL;
static lv_obj_t *g_ai_card_title_2 = NULL;
static lv_obj_t *g_ai_card_title_3 = NULL;
static lv_obj_t *g_ai_card_title_4 = NULL;
static lv_obj_t *g_ai_card_sub_1 = NULL;
static lv_obj_t *g_ai_card_sub_2 = NULL;
static lv_obj_t *g_ai_card_sub_3 = NULL;
static lv_obj_t *g_ai_card_sub_4 = NULL;
static lv_obj_t *g_ai_card_feedback_1 = NULL;
static lv_obj_t *g_ai_card_feedback_2 = NULL;
static lv_obj_t *g_ai_card_feedback_3 = NULL;
static lv_obj_t *g_ai_card_feedback_4 = NULL;
static lv_obj_t *g_ai_mic_timer_arc = NULL;
static lv_obj_t *g_city_textarea = NULL;
static lv_obj_t *g_city_label = NULL;
static lv_obj_t *g_temp_degree_dot = NULL;
static lv_obj_t *g_home_gesture_layer = NULL;
static lv_point_t g_home_touch_start = {0, 0};
static bool g_home_touch_tracking = false;
static bool g_tap_listen_active = false;
static bool g_tap_listen_voice_seen = false;
static uint32_t g_tap_listen_result_until_ms = 0;
static bool g_tap_listen_result_error = false;
static char g_tap_listen_result_text[192] = {0};
static volatile bool g_tap_listen_ui_dirty = false;
static TaskHandle_t g_tap_listen_stt_task = NULL;
static volatile bool g_tap_listen_transcribing = false;
static volatile bool g_tap_listen_task_done = false;
static volatile bool g_tap_listen_task_error = false;
static volatile uint16_t g_tap_listen_task_command = VOICE_COMMAND_NONE;
static uint32_t g_tap_listen_transcribing_since_ms = 0;
static uint32_t g_tap_listen_capture_retry_after_ms = 0;
static char g_tap_listen_task_text[192] = {0};
static bool g_mic_reactive_voice_active = false;
static uint32_t g_mic_reactive_voice_started_ms = 0;
static uint32_t g_mic_reactive_voice_last_ms = 0;
static uint32_t g_mic_reactive_cooldown_until_ms = 0;
static uint32_t g_mic_reactive_cooldown_span_ms = 0;
static uint32_t s_last_donor_vu_push_ms = 0;
static uint32_t s_last_donor_uart_push_ms = 0;
static int g_last_clock_tick_minute_key = -1;
static uint32_t g_last_voice_command_ms = 0;
static bool g_alarm_enabled = false;
static bool g_alarm_ringing = false;
static uint8_t g_alarm_hour = 7;
static uint8_t g_alarm_minute = 0;
static int g_alarm_last_fire_minute_key = -1;
static uint32_t g_alarm_next_ring_ms = 0;
static uint8_t g_alarm_ring_intensity = 0;
static bool g_alarm_restore_volume = false;
static uint8_t g_alarm_saved_volume = 50;
static lv_obj_t *g_alarm_state_label = NULL;
static lv_obj_t *g_alarm_time_label = NULL;
static lv_obj_t *g_alarm_toggle_hitbox = NULL;
static lv_obj_t *g_alarm_time_hitbox = NULL;
static lv_obj_t *g_alarm_minus_hitbox = NULL;
static lv_obj_t *g_alarm_plus_hitbox = NULL;
static lv_obj_t *g_alarm_minus_label = NULL;
static lv_obj_t *g_alarm_plus_label = NULL;
static uint8_t g_alarm_melody = 1;
static lv_obj_t *g_alarm_melody_btn_1 = NULL;
static lv_obj_t *g_alarm_melody_btn_2 = NULL;
static lv_obj_t *g_alarm_melody_btn_3 = NULL;

enum AssistantActionKind
{
    ASSISTANT_ACTION_NONE = 0,
    ASSISTANT_ACTION_SELF,
    ASSISTANT_ACTION_ASK,
    ASSISTANT_ACTION_MURMUR,
    ASSISTANT_ACTION_MIC,
};

static AssistantActionKind g_pending_assistant_action = ASSISTANT_ACTION_NONE;
static uint32_t g_pending_assistant_action_at = 0;
static uint8_t g_pending_assistant_card = 0xFF;

static constexpr lv_coord_t kAssistantCardXs[4] = {-132, 0, 132, 320};
static constexpr lv_coord_t kAssistantCardY = 90;
static constexpr lv_coord_t kAssistantCardWidth = 96;
static constexpr lv_coord_t kAssistantCardHeight = 186;
static constexpr uint32_t kAssistantActionDelayMs = 45;
static constexpr uint32_t kTapListenWindowMs = 15000;
static constexpr uint32_t kTapListenMaxRecordMs = 5200;
static constexpr uint32_t kTapListenSilenceMs = 900;
static constexpr uint32_t kTapListenCaptureRetryMs = 1400;
static constexpr uint32_t kTapListenResultHoldMs = 15000;
static constexpr uint32_t kTapListenTranscribeTimeoutMs = 22000;
static const char *kTapListenWavPath = "/tap_cmd.wav";
static constexpr bool kDonorSoundWakeEnabled = true;
static constexpr bool kCityNameGeocodingEnabled = false;
static const char *const kPlayerMusicFolders[] = {
    "/player/ua_estrada/50s",
    "/player/ua_estrada/60s",
    "/player/ua_estrada/70s",
};
static constexpr uint32_t kAlarmRingRepeatMs = 3500;
static constexpr int kAlarmAdjustStepMinutes = 5;
static constexpr int kAlarmAdjustStepFastMinutes = 20;

static void schedule_next_ambient(bool initial = false);
static void note_user_activity();
static bool current_screen_allows_ambient();
static bool play_sd_action(const char *kind);
static void load_saved_wifi_credentials();
static void save_wifi_credentials();
static void apply_brightness_slider();
static void apply_runtime_backlight(uint8_t level);
static bool media_output_active();
static bool mic_sound_above_wake_level();
static void refresh_display_sleep_state();
static void update_home_labels();
static void update_settings_labels();
static void update_ai_labels();
static void update_weather_icon(bool reset_frame);
static void update_home_background();
static void enable_home_gesture_bubble();
static void ensure_panel_link_ap();
static void configure_assistant_card_ui();
static void assistant_card_event_cb(lv_event_t *e);
static void queue_assistant_action(AssistantActionKind action);
static void show_assistant_status(const char *message);
static void ensure_boot_black_screen();
static void show_boot_black_screen();
static void ensure_weather_task_running();
static bool fetch_weather_once_now();
static bool start_wifi_connect_task();
static void donor_eq_uart_init();
static void donor_eq_uart_push(const uint8_t *bars, size_t count, bool active);
static SdVoiceMode current_voice_mode();
static void begin_tap_listen();
static void stop_tap_listen(bool remote_stop = true);
static void sync_remote_mic_visual_state(bool force = false);
static void update_tap_listen_visuals();
static void clear_tap_listen_result();
static void set_tap_listen_result(const char *message, bool is_error, uint32_t hold_ms = kTapListenResultHoldMs);
static bool ensure_tap_listen_capture(bool quiet_error = false);
static void start_tap_listen_transcription();
static void tap_listen_transcribe_task(void *pvParameters);
static bool play_mic_reactive_response(uint32_t voice_duration_ms);
static void execute_voice_command(uint16_t command_id);
static void step_voice_screen(int delta);
static void show_voice_command_feedback(uint16_t command_id);
static int alarm_minute_key();
static void update_alarm_labels();
static void save_alarm_settings();
static void alarm_toggle_event_cb(lv_event_t *e);
static void alarm_adjust_minutes(int delta_minutes);
static void alarm_minus_event_cb(lv_event_t *e);
static void alarm_plus_event_cb(lv_event_t *e);
static void alarm_melody_event_cb(lv_event_t *e);
static void alarm_stop_ringing();
static void alarm_check_and_handle();
static void update_alarm_melody_buttons();
static const char *alarm_folder_for_melody(uint8_t melody_id);
static void set_assistant_card_feedback(uint8_t index, bool active);
static bool system_audio_actions_allowed();
static void set_system_soft_off(bool enabled);
static bool clock_time_valid();

static lv_obj_t *assistant_feedback_for_card(uint8_t index)
{
    switch (index)
    {
        case 0: return g_ai_card_feedback_1;
        case 1: return g_ai_card_feedback_2;
        case 2: return g_ai_card_feedback_3;
        case 3: return g_ai_card_feedback_4;
        default: return NULL;
    }
}

static lv_obj_t *assistant_button_for_card(uint8_t index)
{
    switch (index)
    {
        case 0: return ui_Button3;
        case 1: return ui_Button4;
        case 2: return ui_Button5;
        case 3: return ui_Button6;
        default: return NULL;
    }
}

static lv_obj_t *assistant_image_for_card(uint8_t index)
{
    switch (index)
    {
        case 0: return ui_Image64;
        case 1: return ui_Image65;
        case 2: return ui_Image66;
        case 3: return ui_Image67;
        default: return NULL;
    }
}

static uint8_t assistant_card_for_action(AssistantActionKind action)
{
    switch (action)
    {
        case ASSISTANT_ACTION_SELF: return 0;
        case ASSISTANT_ACTION_ASK: return 1;
        case ASSISTANT_ACTION_MURMUR: return 2;
        case ASSISTANT_ACTION_MIC: return 3;
        default: return 0xFF;
    }
}

static const char *assistant_pending_status(AssistantActionKind action)
{
    switch (action)
    {
        case ASSISTANT_ACTION_SELF: return g_sound_mode_self_enabled ? "Self mode on" : "Self mode off";
        case ASSISTANT_ACTION_ASK: return g_sound_mode_ask_enabled ? "Prompt mode on" : "Prompt mode off";
        case ASSISTANT_ACTION_MURMUR: return g_sound_mode_murmur_enabled ? "Ambient mode on" : "Ambient mode off";
        case ASSISTANT_ACTION_MIC: return "Mic disabled";
        default: return "";
    }
}

static bool assistant_action_enabled(AssistantActionKind action)
{
    switch (action)
    {
        case ASSISTANT_ACTION_SELF: return g_sound_mode_self_enabled;
        case ASSISTANT_ACTION_ASK: return g_sound_mode_ask_enabled;
        case ASSISTANT_ACTION_MURMUR: return g_sound_mode_murmur_enabled;
        case ASSISTANT_ACTION_MIC: return false;
        default: return false;
    }
}

static bool assistant_card_enabled(uint8_t index)
{
    switch (index)
    {
        case 0: return g_sound_mode_self_enabled;
        case 1: return g_sound_mode_ask_enabled;
        case 2: return g_sound_mode_murmur_enabled;
        case 3: return false;
        default: return false;
    }
}

static void refresh_assistant_card_feedbacks()
{
    for (uint8_t i = 0; i < 4; ++i)
    {
        set_assistant_card_feedback(i, assistant_card_enabled(i));
    }
}

static uint8_t collect_enabled_sound_actions(AssistantActionKind *actions, uint8_t capacity)
{
    uint8_t count = 0;
    if (g_sound_mode_self_enabled && count < capacity)
    {
        actions[count++] = ASSISTANT_ACTION_SELF;
    }
    if (g_sound_mode_ask_enabled && count < capacity)
    {
        actions[count++] = ASSISTANT_ACTION_ASK;
    }
    if (g_sound_mode_murmur_enabled && count < capacity)
    {
        actions[count++] = ASSISTANT_ACTION_MURMUR;
    }
    return count;
}

static void set_assistant_card_feedback(uint8_t index, bool active)
{
    lv_obj_t *feedback = assistant_feedback_for_card(index);
    lv_obj_t *button = assistant_button_for_card(index);
    lv_obj_t *image = assistant_image_for_card(index);
    if (!feedback)
    {
        return;
    }

    if (active)
    {
        _ui_flag_modify(feedback, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
    else
    {
        _ui_flag_modify(feedback, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }

    if (button)
    {
        lv_obj_set_style_bg_opa(button, active ? 118 : LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(button, active ? 3 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(button, active ? 24 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (image)
    {
        lv_img_set_zoom(image, active ? 248 : 230);
    }
}

static void clear_assistant_card_feedback()
{
    for (uint8_t i = 0; i < 4; ++i)
    {
        set_assistant_card_feedback(i, false);
    }
}

static void clear_tap_listen_result()
{
    g_tap_listen_result_text[0] = '\0';
    g_tap_listen_result_error = false;
    g_tap_listen_result_until_ms = 0;
    g_tap_listen_ui_dirty = true;
}

static void set_tap_listen_result(const char *message, bool is_error, uint32_t hold_ms)
{
    snprintf(g_tap_listen_result_text, sizeof(g_tap_listen_result_text), "%s", message ? message : "");
    g_tap_listen_result_error = is_error;
    g_tap_listen_result_until_ms = hold_ms ? (millis() + hold_ms) : 0;
    g_tap_listen_ui_dirty = true;
}

static bool ensure_tap_listen_capture(bool quiet_error)
{
    if (!g_tap_listen_active || g_tap_listen_transcribing)
    {
        return false;
    }
    if (mic_remote_client_listen_active() || mic_remote_client_clip_ready())
    {
        return true;
    }

    const uint32_t now = millis();
    if (now < g_tap_listen_capture_retry_after_ms)
    {
        return false;
    }

    if (!mic_remote_client_listen_start(kTapListenWindowMs, kTapListenMaxRecordMs, kTapListenSilenceMs))
    {
        g_tap_listen_capture_retry_after_ms = now + kTapListenCaptureRetryMs;
        if (!quiet_error)
        {
            set_tap_listen_result("Mic listen retry", true, 1800);
        }
        return false;
    }

    g_tap_listen_capture_retry_after_ms = 0;
    return true;
}

static void tap_listen_transcribe_task(void *pvParameters)
{
    (void)pvParameters;

    char transcript[sizeof(g_tap_listen_task_text)] = {0};
    char result[sizeof(g_tap_listen_task_text)] = {0};
    uint16_t commandId = VOICE_COMMAND_NONE;
    bool error = false;

    mic_remote_client_suspend_polling(true);
    stt_cloud_set_language_override((g_voice_command_language == VOICE_LANG_PT_PT) ? "pt-PT" : "en");

    if (!mic_remote_client_download_wav(kTapListenWavPath))
    {
        snprintf(result, sizeof(result), "Audio download failed");
        error = true;
    }
    else if (!stt_cloud_configured())
    {
        snprintf(result, sizeof(result), "STT not configured");
        error = true;
    }
    else if (!stt_cloud_transcribe_sd_wav(kTapListenWavPath, transcript, sizeof(transcript)))
    {
        const char *sttError = stt_cloud_last_error();
        snprintf(result, sizeof(result), "%s", (sttError && sttError[0]) ? sttError : "Transcription failed");
        error = true;
    }
    else
    {
        const char *langCode = (g_voice_command_language == VOICE_LANG_PT_PT) ? "pt-PT" : "en";
        commandId = voice_command_match_transcript(transcript, langCode);
        if (commandId != VOICE_COMMAND_NONE)
        {
            snprintf(result, sizeof(result), "Cmd: %s", voice_command_name(commandId));
        }
        else
        {
            snprintf(result, sizeof(result), "No command");
        }
    }

    mic_remote_client_suspend_polling(false);

    g_tap_listen_task_command = commandId;
    g_tap_listen_task_error = error;
    snprintf(g_tap_listen_task_text, sizeof(g_tap_listen_task_text), "%s", result);
    g_tap_listen_task_done = true;
    g_tap_listen_transcribing = false;
    g_tap_listen_transcribing_since_ms = 0;
    g_tap_listen_stt_task = NULL;
    vTaskDelete(NULL);
}

static void start_tap_listen_transcription()
{
    if (!g_tap_listen_active || g_tap_listen_transcribing || g_tap_listen_stt_task != NULL)
    {
        return;
    }

    g_tap_listen_transcribing = true;
    g_tap_listen_transcribing_since_ms = millis();
    g_tap_listen_task_done = false;
    g_tap_listen_task_error = false;
    g_tap_listen_task_command = VOICE_COMMAND_NONE;
    g_tap_listen_task_text[0] = '\0';

    if (xTaskCreatePinnedToCore(tap_listen_transcribe_task, "tap_cmd_stt", 16384, NULL, 1, &g_tap_listen_stt_task, 1) != pdPASS)
    {
        g_tap_listen_transcribing = false;
        g_tap_listen_transcribing_since_ms = 0;
        g_tap_listen_stt_task = NULL;
        set_tap_listen_result("Mic task start failed", true, 5000);
        mic_remote_client_suspend_polling(false);
    }
}
static SdVoiceMode current_voice_mode()
{
    if (currentTime_hour >= 23 || currentTime_hour < 5)
    {
        return SD_VOICE_NIGHT;
    }
    if (currentTime_hour >= 17)
    {
        return SD_VOICE_EVENING;
    }
    return SD_VOICE_DAY;
}

static const char *voice_command_language_code()
{
    return (g_voice_command_language == VOICE_LANG_PT_PT) ? "pt-PT" : "en";
}

static const char *voice_command_language_label()
{
    return "Mic mode";
}

static const char *voice_command_language_value_label()
{
    return "Mic off";
}

static void toggle_voice_command_language()
{
    g_ai_voice_enabled = false;
    stop_tap_listen();
    set_tap_listen_result("Mic disabled", false, 1800);
    update_settings_labels();
    update_home_labels();
}

static void settings_voice_language_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }
    note_user_activity();
    toggle_voice_command_language();
}

static int alarm_minute_key()
{
    // Stable key for one trigger per matching minute (day + hour + minute), fits int32.
    return ((((static_cast<int>(currentTime_year) * 13) + static_cast<int>(currentTime_mouth)) * 32 +
             static_cast<int>(currentTime_day)) *
                24 +
            static_cast<int>(currentTime_hour)) *
               60 +
           static_cast<int>(currentTime_minute);
}

static bool clock_time_valid()
{
    return currentTime_year >= 2024 && currentTime_year <= 2099 &&
           currentTime_mouth >= 1 && currentTime_mouth <= 12 &&
           currentTime_day >= 1 && currentTime_day <= 31;
}

static void save_alarm_settings()
{
    if (!g_prefs.begin("wifi", false))
    {
        return;
    }
    g_prefs.putBool("alarm_on", g_alarm_enabled);
    g_prefs.putUChar("alarm_h", g_alarm_hour);
    g_prefs.putUChar("alarm_m", g_alarm_minute);
    g_prefs.putUChar("alarm_tone", g_alarm_melody);
    g_prefs.end();
}

static void alarm_stop_ringing()
{
    if (g_alarm_ringing)
    {
        radio_ctrl_stop();
        speaker_tone_stop();
    }
    if (g_alarm_restore_volume)
    {
        radio_vendor_set_volume_percent(g_alarm_saved_volume);
        g_alarm_restore_volume = false;
    }
    g_alarm_ringing = false;
    g_alarm_next_ring_ms = 0;
    g_alarm_ring_intensity = 0;
}

static void update_alarm_labels()
{
    if (g_alarm_state_label)
    {
        const char *stateText = g_alarm_ringing ? "Alarm RINGING" : (g_alarm_enabled ? "Alarm ON" : "Alarm OFF");
        lv_label_set_text(g_alarm_state_label, stateText);
        lv_obj_set_style_text_color(g_alarm_state_label,
                                    g_alarm_ringing ? lv_color_hex(0xFFC080) : lv_color_hex(0xDDE8FF),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_alarm_time_label)
    {
        char timeText[16] = {0};
        snprintf(timeText, sizeof(timeText), "%02u:%02u",
                 static_cast<unsigned>(g_alarm_hour),
                 static_cast<unsigned>(g_alarm_minute));
        lv_label_set_text(g_alarm_time_label, timeText);
    }

    update_alarm_melody_buttons();
}

static bool is_alarm_input_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    return (code == LV_EVENT_CLICKED || code == LV_EVENT_RELEASED);
}

static void alarm_toggle_event_cb(lv_event_t *e)
{
    if (!is_alarm_input_event(e))
    {
        return;
    }
    note_user_activity();

    if (g_alarm_ringing)
    {
        alarm_stop_ringing();
    }
    else
    {
        g_alarm_enabled = !g_alarm_enabled;
    }

    save_alarm_settings();
    update_alarm_labels();
}

static void alarm_adjust_minutes(int delta_minutes)
{
    const int dayMinutes = 24 * 60;
    const int total = static_cast<int>(g_alarm_hour) * 60 + static_cast<int>(g_alarm_minute);
    int next = (total + delta_minutes) % dayMinutes;
    if (next < 0)
    {
        next += dayMinutes;
    }

    g_alarm_hour = static_cast<uint8_t>(next / 60);
    g_alarm_minute = static_cast<uint8_t>(next % 60);
    save_alarm_settings();
    update_alarm_labels();
}

static int alarm_adjust_delta_for_event(lv_event_t *e, int direction)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_LONG_PRESSED_REPEAT)
    {
        return direction * kAlarmAdjustStepFastMinutes;
    }
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_RELEASED)
    {
        return direction * kAlarmAdjustStepMinutes;
    }
    return 0;
}

static void alarm_minus_event_cb(lv_event_t *e)
{
    const int delta = alarm_adjust_delta_for_event(e, -1);
    if (delta == 0)
    {
        return;
    }
    note_user_activity();
    alarm_adjust_minutes(delta);
}

static void alarm_plus_event_cb(lv_event_t *e)
{
    const int delta = alarm_adjust_delta_for_event(e, 1);
    if (delta == 0)
    {
        return;
    }
    note_user_activity();
    alarm_adjust_minutes(delta);
}

static const char *alarm_folder_for_melody(uint8_t melody_id)
{
    switch (melody_id)
    {
        case 1: return "/alarm1";
        case 2: return "/alarm2";
        case 3: return "/alarm3";
        default: return "/alarm";
    }
}

static void update_alarm_melody_buttons()
{
    struct MelodyButton
    {
        lv_obj_t *btn;
        uint8_t id;
    };

    const MelodyButton buttons[] = {
        {g_alarm_melody_btn_1, 1},
        {g_alarm_melody_btn_2, 2},
        {g_alarm_melody_btn_3, 3},
    };

    for (const MelodyButton &entry : buttons)
    {
        if (!entry.btn)
        {
            continue;
        }
        const bool active = (g_alarm_melody == entry.id);
        lv_obj_set_style_bg_opa(entry.btn, active ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(entry.btn, active ? lv_color_hex(0x2D7FAE) : lv_color_hex(0x2D4864), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(entry.btn, active ? lv_color_hex(0xAEEBFF) : lv_color_hex(0x5E86A8), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void alarm_melody_event_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED)
    {
        return;
    }

    const uint8_t requested = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (requested < 1 || requested > 3)
    {
        return;
    }

    note_user_activity();
    if (g_alarm_melody != requested)
    {
        g_alarm_melody = requested;
        save_alarm_settings();
    }
    update_alarm_labels();
}

static void alarm_check_and_handle()
{
    if (!clock_time_valid())
    {
        return;
    }
    if (g_alarm_ringing)
    {
        const uint32_t nowMs = millis();
        if (nowMs >= g_alarm_next_ring_ms)
        {
            if (radio_vendor_is_playing())
            {
                radio_ctrl_stop();
            }

            if (!sd_vendor_is_busy() && !radio_vendor_is_playing())
            {
                if (!g_alarm_restore_volume)
                {
                    g_alarm_saved_volume = radio_vendor_get_volume_percent();
                    g_alarm_restore_volume = true;
                }

                const uint8_t rampVolume = (uint8_t)min(95, 22 + (int)g_alarm_ring_intensity * 8);
                radio_vendor_set_volume_percent(rampVolume);

                const char *alarmFolder = alarm_folder_for_melody(g_alarm_melody);
                bool played = sd_vendor_play_random(alarmFolder);
                if (!played && strcmp(alarmFolder, "/alarm") != 0)
                {
                    played = sd_vendor_play_random("/alarm");
                }
                if (!played)
                {
                    played = speaker_tone_alarm_variant(g_alarm_melody, g_alarm_ring_intensity);
                }
                if (!played)
                {
                    played = sd_vendor_play_ack_mode(current_voice_mode());
                }
                if (!played)
                {
                    (void)sd_vendor_play_listening_mode(current_voice_mode());
                }

                if (g_alarm_ring_intensity < 9U)
                {
                    ++g_alarm_ring_intensity;
                }
            }

            g_alarm_next_ring_ms = nowMs + kAlarmRingRepeatMs;
        }
        return;
    }

    if (!g_alarm_enabled)
    {
        return;
    }

    if (currentTime_hour == g_alarm_hour && currentTime_minute == g_alarm_minute)
    {
        const int minuteKey = alarm_minute_key();
        if (minuteKey != g_alarm_last_fire_minute_key)
        {
            g_alarm_last_fire_minute_key = minuteKey;
            if (g_system_soft_off)
            {
                set_system_soft_off(false);
            }
            g_alarm_ringing = true;
            g_alarm_next_ring_ms = 0;
            g_alarm_ring_intensity = 0;
            g_alarm_saved_volume = radio_vendor_get_volume_percent();
            g_alarm_restore_volume = true;
            update_alarm_labels();
        }
    }
}
struct VoiceScreenTarget
{
    lv_obj_t **screen;
    void (*init)();
};

static void change_voice_screen(const VoiceScreenTarget &target)
{
    if (!target.screen || !*target.screen || !target.init)
    {
        return;
    }
    if (lv_scr_act() == *target.screen)
    {
        return;
    }
    _ui_screen_change(target.screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, target.init);
}

static int current_voice_screen_index()
{
    static const VoiceScreenTarget kScreens[] = {
        {&ui_Screen1, &ui_Screen1_screen_init},
        {&ui_Screen10, &ui_Screen10_screen_init},
        {&ui_Screen11, &ui_Screen11_screen_init},
        {&ui_Screen12, &ui_Screen12_screen_init},
        {&ui_Screen13, &ui_Screen13_screen_init},
    };

    lv_obj_t *active = lv_scr_act();
    for (size_t i = 0; i < (sizeof(kScreens) / sizeof(kScreens[0])); ++i)
    {
        if (active == *kScreens[i].screen)
        {
            return static_cast<int>(i);
        }
    }
    return 0;
}

static void step_voice_screen(int delta)
{
    static const VoiceScreenTarget kScreens[] = {
        {&ui_Screen1, &ui_Screen1_screen_init},
        {&ui_Screen10, &ui_Screen10_screen_init},
        {&ui_Screen11, &ui_Screen11_screen_init},
        {&ui_Screen12, &ui_Screen12_screen_init},
        {&ui_Screen13, &ui_Screen13_screen_init},
    };

    const int count = static_cast<int>(sizeof(kScreens) / sizeof(kScreens[0]));
    int index = current_voice_screen_index();
    index = (index + delta + count) % count;
    change_voice_screen(kScreens[index]);
}

static void show_voice_command_feedback(uint16_t command_id)
{
    if (command_id == VOICE_COMMAND_NONE)
    {
        return;
    }

    char text[64];
    snprintf(text, sizeof(text), "Cmd: %s", voice_command_name(command_id));
    set_tap_listen_result(text, false, 1800);
}

static void execute_voice_command(uint16_t command_id)
{
    if (command_id == VOICE_COMMAND_NONE)
    {
        return;
    }

    g_last_voice_command_ms = millis();
    note_user_activity();
    show_voice_command_feedback(command_id);

    switch (command_id)
    {
        case VOICE_COMMAND_GO_HOME:
            change_voice_screen({&ui_Screen1, &ui_Screen1_screen_init});
            break;
        case VOICE_COMMAND_OPEN_RADIO:
            change_voice_screen({&ui_Screen11, &ui_Screen11_screen_init});
            break;
        case VOICE_COMMAND_OPEN_SETTINGS:
            change_voice_screen({&ui_Screen10, &ui_Screen10_screen_init});
            break;
        case VOICE_COMMAND_OPEN_ASSISTANT:
            change_voice_screen({&ui_Screen12, &ui_Screen12_screen_init});
            break;
        case VOICE_COMMAND_OPEN_CLOCK:
            change_voice_screen({&ui_Screen13, &ui_Screen13_screen_init});
            break;
        case VOICE_COMMAND_NEXT_SCREEN:
            step_voice_screen(1);
            break;
        case VOICE_COMMAND_PREVIOUS_SCREEN:
            step_voice_screen(-1);
            break;
        case VOICE_COMMAND_PLAY_HITS:
            radio_vendor_start_index(0);
            change_voice_screen({&ui_Screen11, &ui_Screen11_screen_init});
            break;
        case VOICE_COMMAND_PLAY_LOUNGE:
            radio_vendor_start_index(1);
            change_voice_screen({&ui_Screen11, &ui_Screen11_screen_init});
            break;
        case VOICE_COMMAND_PLAY_PORTUGAL:
            radio_vendor_start_index(2);
            change_voice_screen({&ui_Screen11, &ui_Screen11_screen_init});
            break;
        case VOICE_COMMAND_STOP_RADIO:
            radio_ctrl_stop();
            break;
        case VOICE_COMMAND_VOLUME_UP:
        {
            const uint8_t volume = static_cast<uint8_t>(min<int>(100, static_cast<int>(radio_vendor_get_volume_percent()) + 10));
            radio_vendor_set_volume_percent(volume);
            lv_slider_set_value(ui_Slider1, volume, LV_ANIM_OFF);
            break;
        }
        case VOICE_COMMAND_VOLUME_DOWN:
        {
            const uint8_t volume = static_cast<uint8_t>(max<int>(0, static_cast<int>(radio_vendor_get_volume_percent()) - 10));
            radio_vendor_set_volume_percent(volume);
            lv_slider_set_value(ui_Slider1, volume, LV_ANIM_OFF);
            break;
        }
        case VOICE_COMMAND_BRIGHTNESS_UP:
        {
            const int slider = min<int>(100, static_cast<int>(map(g_backlight, 20, 255, 0, 100)) + 10);
            lv_slider_set_value(ui_Slider2, slider, LV_ANIM_OFF);
            apply_brightness_slider();
            break;
        }
        case VOICE_COMMAND_BRIGHTNESS_DOWN:
        {
            const int slider = max<int>(0, static_cast<int>(map(g_backlight, 20, 255, 0, 100)) - 10);
            lv_slider_set_value(ui_Slider2, slider, LV_ANIM_OFF);
            apply_brightness_slider();
            break;
        }
        case VOICE_COMMAND_SELF:
            queue_assistant_action(ASSISTANT_ACTION_SELF);
            break;
        case VOICE_COMMAND_ASK:
            queue_assistant_action(ASSISTANT_ACTION_ASK);
            break;
        case VOICE_COMMAND_MURMUR:
            queue_assistant_action(ASSISTANT_ACTION_MURMUR);
            break;
        case VOICE_COMMAND_MIC_ON:
            if (!g_tap_listen_active && mic_remote_client_online() && mic_remote_client_ready())
            {
                begin_tap_listen();
            }
            break;
        case VOICE_COMMAND_MIC_OFF:
            if (g_tap_listen_active)
            {
                stop_tap_listen();
            }
            break;
        case VOICE_COMMAND_ALARM_ON:
            g_alarm_enabled = true;
            alarm_stop_ringing();
            save_alarm_settings();
            update_alarm_labels();
            break;
        case VOICE_COMMAND_ALARM_OFF:
            g_alarm_enabled = false;
            alarm_stop_ringing();
            save_alarm_settings();
            update_alarm_labels();
            break;
        default:
            break;
    }

    update_ai_labels();
}
static void begin_tap_listen()
{
    if (media_output_active() || g_alarm_ringing)
    {
        return;
    }
    if (!sd_vendor_ready())
    {
        set_tap_listen_result("SD pack missing", true, 4000);
        update_ai_labels();
        return;
    }
    ensure_panel_link_ap();
    for (uint8_t attempt = 0; attempt < 6; ++attempt)
    {
        mic_remote_client_force_refresh();
        if (mic_remote_client_online() && mic_remote_client_ready())
        {
            break;
        }
        delay(attempt == 0 ? 40 : 120);
        ensure_panel_link_ap();
    }
    if (!mic_remote_client_online() || !mic_remote_client_ready())
    {
        set_tap_listen_result(mic_remote_client_status(), true, 4000);
        update_ai_labels();
        return;
    }


    clear_tap_listen_result();
    g_tap_listen_active = true;
    g_tap_listen_voice_seen = false;
    g_tap_listen_transcribing = false;
    g_tap_listen_transcribing_since_ms = 0;
    g_tap_listen_capture_retry_after_ms = 0;
    g_tap_listen_task_done = false;
    g_tap_listen_task_error = false;
    g_tap_listen_task_command = VOICE_COMMAND_NONE;
    g_tap_listen_task_text[0] = '\0';
    g_mic_reactive_voice_active = false;
    g_mic_reactive_voice_started_ms = 0;
    g_mic_reactive_voice_last_ms = 0;
    g_mic_reactive_cooldown_until_ms = 0;
    g_mic_reactive_cooldown_span_ms = 0;

    g_last_sound_activity_ms = millis();
    set_tap_listen_result("Mic on", false, 1800);
    mic_remote_client_set_visual_armed();
    update_tap_listen_visuals();
    update_home_labels();
}

static void stop_tap_listen(bool remote_stop)
{
    if (remote_stop && (g_tap_listen_active || mic_remote_client_listen_active() || mic_remote_client_clip_ready()))
    {
        mic_remote_client_listen_stop();
    }

    if (g_tap_listen_stt_task != NULL)
    {
        vTaskDelete(g_tap_listen_stt_task);
        g_tap_listen_stt_task = NULL;
    }

    mic_remote_client_suspend_polling(false);

    g_tap_listen_active = false;
    g_tap_listen_voice_seen = false;
    g_tap_listen_transcribing = false;
    g_tap_listen_transcribing_since_ms = 0;
    g_tap_listen_capture_retry_after_ms = 0;
    g_tap_listen_task_done = false;
    g_tap_listen_task_error = false;
    g_tap_listen_task_command = VOICE_COMMAND_NONE;
    g_tap_listen_task_text[0] = '\0';
    g_mic_reactive_voice_active = false;
    g_mic_reactive_voice_started_ms = 0;
    g_mic_reactive_voice_last_ms = 0;
    g_mic_reactive_cooldown_until_ms = 0;
    g_mic_reactive_cooldown_span_ms = 0;
    mic_remote_client_set_visual_idle();
    update_tap_listen_visuals();
    update_home_labels();
    if (g_pending_assistant_card != 3)
    {
        set_assistant_card_feedback(3, false);
    }
    sync_remote_mic_visual_state(true);
}

static void sync_remote_mic_visual_state(bool force)
{
    static uint32_t s_last_disabled_push_ms = 0;
    const bool shouldDisable = g_system_soft_off || !g_ai_voice_enabled || media_output_active();
    if (!shouldDisable)
    {
        s_last_disabled_push_ms = 0;
        return;
    }
    if (!mic_remote_client_online())
    {
        s_last_disabled_push_ms = 0;
        return;
    }

    const uint32_t now = millis();
    if (!force && (now - s_last_disabled_push_ms) < 1200UL)
    {
        return;
    }

    mic_remote_client_set_visual_disabled();
    s_last_disabled_push_ms = now;
}

static void update_tap_listen_visuals()
{
    if (!g_ai_mic_timer_arc)
    {
        return;
    }

    if (!g_tap_listen_active)
    {
        _ui_flag_modify(g_ai_mic_timer_arc, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        lv_arc_set_value(g_ai_mic_timer_arc, 0);
        return;
    }

    const uint32_t now = millis();
    const bool cooling_down = now < g_mic_reactive_cooldown_until_ms;
    lv_color_t color = lv_color_hex(0xDDE8FF);
    uint16_t value = 100;

    if (g_mic_reactive_voice_active)
    {
        color = lv_color_hex(0x95FFF0);
    }
    else if (cooling_down)
    {
        color = lv_color_hex(0xFFD38A);
        if (g_mic_reactive_cooldown_span_ms > 0)
        {
            const uint32_t remaining = g_mic_reactive_cooldown_until_ms - now;
            value = (uint16_t)((remaining * 100UL) / g_mic_reactive_cooldown_span_ms);
        }
    }

    _ui_flag_modify(g_ai_mic_timer_arc, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    lv_arc_set_value(g_ai_mic_timer_arc, value > 100 ? 100 : value);
    lv_obj_set_style_arc_color(g_ai_mic_timer_arc, color, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

static void home_gesture_layer_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST)
    {
        return;
    }

    lv_indev_t *indev = lv_event_get_indev(e);
    if (!indev)
    {
        indev = lv_indev_get_act();
    }
    if (!indev)
    {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED)
    {
        g_home_touch_start = point;
        g_home_touch_tracking = true;
        return;
    }

    if (code == LV_EVENT_PRESS_LOST)
    {
        g_home_touch_tracking = false;
        return;
    }

    if (!g_home_touch_tracking)
    {
        return;
    }

    g_home_touch_tracking = false;
    const lv_coord_t dx = point.x - g_home_touch_start.x;
    const lv_coord_t dy = point.y - g_home_touch_start.y;
    const bool edge_tap = LV_ABS(dx) < 24 && LV_ABS(dy) < 24;
    if (edge_tap)
    {
        if (g_home_touch_start.x >= 380)
        {
            step_voice_screen(1);
        }
        else if (g_home_touch_start.x <= 100)
        {
            step_voice_screen(-1);
        }
        return;
    }

    if (LV_ABS(dx) < 36 || LV_ABS(dx) <= (LV_ABS(dy) + 10))
    {
        return;
    }

    step_voice_screen(dx < 0 ? 1 : -1);
}

static lv_obj_t *create_overlay_label(lv_obj_t *parent, lv_coord_t width, const lv_font_t *font,
                                      lv_color_t color, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

static void ensure_boot_black_screen()
{
    if (g_boot_black_screen)
    {
        return;
    }

    g_boot_black_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(g_boot_black_screen);
    lv_obj_clear_flag(g_boot_black_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(g_boot_black_screen, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_boot_black_screen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void show_boot_black_screen()
{
    if (!g_boot_wait_wifi_on_start || g_boot_black_active)
    {
        return;
    }

    ensure_boot_black_screen();
    _ui_screen_change(&g_boot_black_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, NULL);
    g_boot_black_active = true;
}

static void home_shortcut_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_RELEASED && code != LV_EVENT_CLICKED)
    {
        return;
    }

    note_user_activity();
    change_voice_screen({&ui_Screen1, &ui_Screen1_screen_init});
}

static lv_obj_t *create_home_shortcut(lv_obj_t *parent)
{
    if (!parent)
    {
        return NULL;
    }

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, 64, 30);
    lv_obj_align(button, LV_ALIGN_TOP_LEFT, 12, 12);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_bg_opa(button, LV_OPA_80, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x162231), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x20344A), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(button, lv_color_hex(0x5E86A8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(button, lv_color_hex(0xAEEBFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(button, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(button, 12, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, home_shortcut_event, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(button, home_shortcut_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "Home");
    lv_obj_set_style_text_font(label, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(label, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_move_foreground(button);
    return button;
}


static void enable_gesture_bubble_recursive(lv_obj_t *obj)
{
    if (!obj)
    {
        return;
    }

    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);

    const uint32_t child_count = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < child_count; ++i)
    {
        enable_gesture_bubble_recursive(lv_obj_get_child(obj, i));
    }
}

static void enable_home_gesture_bubble()
{
    if (!ui_Screen1)
    {
        return;
    }

    enable_gesture_bubble_recursive(ui_Screen1);
    if (ui_Image1)
    {
        lv_obj_clear_flag(ui_Image1, LV_OBJ_FLAG_CLICKABLE);
    }

    if (!g_home_gesture_layer)
    {
        g_home_gesture_layer = lv_obj_create(ui_Screen1);
        lv_obj_remove_style_all(g_home_gesture_layer);
        lv_obj_set_size(g_home_gesture_layer, LV_PCT(100), LV_PCT(100));
        lv_obj_align(g_home_gesture_layer, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(g_home_gesture_layer, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_home_gesture_layer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(g_home_gesture_layer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(g_home_gesture_layer, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(g_home_gesture_layer, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_home_gesture_layer, home_gesture_layer_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_home_gesture_layer, home_gesture_layer_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_home_gesture_layer, home_gesture_layer_cb, LV_EVENT_PRESS_LOST, NULL);
    }

    lv_obj_move_foreground(g_home_gesture_layer);
}

static void load_saved_wifi_credentials()
{
    WifiManagerStoredSettings settings;
    if (!wifi_manager_load_preferences(g_prefs, &settings))
    {
        Serial.println("[wifi] prefs open failed (read)");
        return;
    }

    ssid = settings.ssid;
    pswd = settings.password;
    g_city_query = settings.city_query;
    g_weather_saved_lat = settings.weather_saved_lat;
    g_weather_saved_lon = settings.weather_saved_lon;
    g_voice_command_language = static_cast<VoiceCommandLanguage>(settings.voice_command_language);
    if (g_voice_command_language != VOICE_LANG_EN && g_voice_command_language != VOICE_LANG_PT_PT)
    {
        g_voice_command_language = VOICE_LANG_EN;
    }
    g_alarm_enabled = settings.alarm_enabled;
    g_alarm_hour = settings.alarm_hour;
    g_alarm_minute = settings.alarm_minute;
    g_alarm_melody = settings.alarm_melody;
    if (g_alarm_hour > 23)
    {
        g_alarm_hour = 7;
    }
    if (g_alarm_minute > 59)
    {
        g_alarm_minute = 0;
    }
    if (g_alarm_melody < 1 || g_alarm_melody > 3)
    {
        g_alarm_melody = 1;
    }

    if (ssid.length() > 0)
    {
        Serial.printf("[wifi] loaded saved ssid: %s\n", ssid.c_str());
    }
    else
    {
        Serial.println("[wifi] no saved credentials");
    }
}

static void save_wifi_credentials()
{

    if (g_city_textarea)
    {
        g_city_query = String(lv_textarea_get_text(g_city_textarea));
        g_city_query.trim();
    }

    WifiManagerStoredSettings settings;
    settings.ssid = ssid;
    settings.password = pswd;
    settings.city_query = g_city_query;
    settings.weather_saved_lat = g_weather_saved_lat;
    settings.weather_saved_lon = g_weather_saved_lon;
    settings.voice_command_language = static_cast<uint8_t>(g_voice_command_language);
    settings.alarm_enabled = g_alarm_enabled;
    settings.alarm_hour = g_alarm_hour;
    settings.alarm_minute = g_alarm_minute;
    settings.alarm_melody = g_alarm_melody;

    if (!wifi_manager_save_preferences(g_prefs, &settings))
    {
        Serial.println("[wifi] prefs open failed (write)");
        return;
    }

    if (ssid != settings.ssid)
    {
        ssid = settings.ssid;
    }
    if (pswd != settings.password)
    {
        pswd = settings.password;
    }
    Serial.println("[wifi] credentials saved");
}

static void setup_wifi_city_input()
{
    if (g_city_textarea)
    {
        wifi_ui_apply_saved_city_text(g_city_textarea, g_city_query);
        return;
    }

    if (ui_Image63)
    {
        lv_obj_add_flag(ui_Image63, LV_OBJ_FLAG_HIDDEN);
    }

    g_city_label = lv_label_create(ui_Screen2);
    lv_label_set_text(g_city_label, "City (EN)");
    lv_obj_set_style_text_font(g_city_label, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_city_label, lv_color_hex(0x202020), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_x(g_city_label, -132);
    lv_obj_set_y(g_city_label, -2);
    lv_obj_set_align(g_city_label, LV_ALIGN_CENTER);

    g_city_textarea = lv_textarea_create(ui_Screen2);
    lv_obj_set_width(g_city_textarea, 265);
    lv_obj_set_height(g_city_textarea, LV_SIZE_CONTENT);
    lv_obj_set_x(g_city_textarea, -18);
    lv_obj_set_y(g_city_textarea, 28);
    lv_obj_set_align(g_city_textarea, LV_ALIGN_CENTER);
    lv_textarea_set_placeholder_text(g_city_textarea, "Lviv");
    lv_textarea_set_one_line(g_city_textarea, true);
    lv_textarea_set_password_mode(g_city_textarea, false);
    lv_obj_set_style_text_font(g_city_textarea, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_city_textarea, wifi_ui_textarea_focus_cb, LV_EVENT_FOCUSED, ui_Keyboard4);
    wifi_ui_apply_saved_city_text(g_city_textarea, g_city_query);

    create_home_shortcut(ui_Screen2);
}

static String url_encode_component(const String &value)
{
    String out;
    out.reserve(value.length() * 3);
    for (size_t i = 0; i < value.length(); ++i)
    {
        const char c = value[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            out += c;
        }
        else if (c == ' ')
        {
            out += "%20";
        }
        else
        {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            out += hex;
        }
    }
    return out;
}

static bool getCityPositionByName(const String &cityName, float &latValue, float &lonValue, String &resolvedName)
{
    latValue = 0.0f;
    lonValue = 0.0f;
    resolvedName = "";
    if (cityName.length() == 0)
    {
        return false;
    }

    const String url = "http://geocoding-api.open-meteo.com/v1/search?name=" + url_encode_component(cityName) + "&count=1&language=en&format=json";
    String payload;
    NetWorkerHttpOptions net_opts;
    net_opts.insecure_tls = false;
    net_opts.connect_timeout_ms = 5000;
    net_opts.timeout_ms = 7000;
    const int httpCode = net_worker_http_get(url, &payload, &net_opts);
    if (httpCode != HTTP_CODE_OK)
    {
        return false;
    }

    DynamicJsonDocument filter(256);
    filter["results"][0]["latitude"] = true;
    filter["results"][0]["longitude"] = true;
    filter["results"][0]["name"] = true;
    filter["results"][0]["country"] = true;
    filter["results"][0]["country_code"] = true;
    filter["results"][0]["countryCode"] = true;

    DynamicJsonDocument doc(1024);
    const DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (err)
    {
        return false;
    }

    JsonArray results = doc["results"].as<JsonArray>();
    if (results.isNull() || results.size() == 0)
    {
        return false;
    }

    JsonObject first = results[0];
    latValue = first["latitude"].as<float>();
    lonValue = first["longitude"].as<float>();
    resolvedName = first["name"].as<String>();
    const String country = first["country"].as<String>();
    String countryCode = first["country_code"].as<String>();
    if (countryCode.length() == 0)
    {
        countryCode = first["countryCode"].as<String>();
    }
    countryCode.trim();
    countryCode.toUpperCase();
    g_country_name = country;
    g_country_code = countryCode;
    if (country.length() > 0)
    {
        resolvedName += ", ";
        resolvedName += country;
    }
    return true;
}

static time_t make_local_noon(int year, int month, int day)
{
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    return mktime(&t);
}

static bool same_local_date(time_t stamp, int year, int month, int day)
{
    struct tm tmValue = {};
    localtime_r(&stamp, &tmValue);
    return (tmValue.tm_year + 1900) == year && (tmValue.tm_mon + 1) == month && tmValue.tm_mday == day;
}

static bool compute_western_easter(int year, int &month, int &day)
{
    const int a = year % 19;
    const int b = year / 100;
    const int c = year % 100;
    const int d = b / 4;
    const int e = b % 4;
    const int f = (b + 8) / 25;
    const int g = (b - f + 1) / 3;
    const int h = (19 * a + b - d - g + 15) % 30;
    const int i = c / 4;
    const int k = c % 4;
    const int l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m = (a + 11 * h + 22 * l) / 451;
    month = (h + l - 7 * m + 114) / 31;
    day = ((h + l - 7 * m + 114) % 31) + 1;
    return true;
}

static bool compute_orthodox_easter(int year, int &month, int &day)
{
    const int a = year % 4;
    const int b = year % 7;
    const int c = year % 19;
    const int d = (19 * c + 15) % 30;
    const int e = (2 * a + 4 * b - d + 34) % 7;
    const int julianMonth = (d + e + 114) / 31;
    const int julianDay = ((d + e + 114) % 31) + 1;
    time_t julianNoon = make_local_noon(year, julianMonth, julianDay);
    time_t gregorianNoon = julianNoon + (13 * 86400);
    struct tm tmValue = {};
    localtime_r(&gregorianNoon, &tmValue);
    month = tmValue.tm_mon + 1;
    day = tmValue.tm_mday;
    return true;
}

static String get_religious_holiday_text(int year, int month, int day)
{
    struct HolidayCandidate
    {
        const char *label;
        time_t when;
    };

    HolidayCandidate candidates[10];
    int count = 0;

    candidates[count++] = {"Catholic Christmas", make_local_noon(year, 12, 25)};
    candidates[count++] = {"Catholic Epiphany", make_local_noon(year, 1, 6)};
    candidates[count++] = {"Orthodox Christmas", make_local_noon(year, 1, 7)};

    int easterMonth = 0;
    int easterDay = 0;
    if (compute_western_easter(year, easterMonth, easterDay))
    {
        time_t easter = make_local_noon(year, easterMonth, easterDay);
        candidates[count++] = {"Catholic Good Friday", easter - (2 * 86400)};
        candidates[count++] = {"Catholic Easter", easter};
        candidates[count++] = {"Catholic Easter Monday", easter + 86400};
    }
    if (compute_orthodox_easter(year, easterMonth, easterDay))
    {
        time_t easter = make_local_noon(year, easterMonth, easterDay);
        candidates[count++] = {"Orthodox Good Friday", easter - (2 * 86400)};
        candidates[count++] = {"Orthodox Easter", easter};
        candidates[count++] = {"Orthodox Easter Monday", easter + 86400};
    }

    const time_t today = make_local_noon(year, month, day);
    const time_t horizon = today + (60 * 86400);
    const HolidayCandidate *best = nullptr;
    for (int i = 0; i < count; ++i)
    {
        if (candidates[i].when < today || candidates[i].when > horizon)
        {
            continue;
        }
        if (!best || candidates[i].when < best->when)
        {
            best = &candidates[i];
        }
    }

    if (!best)
    {
        return String("");
    }

    if (same_local_date(best->when, year, month, day))
    {
        return String("Today: ") + best->label;
    }

    return String("Next: ") + best->label;
}

static bool fetch_country_holiday_text(int year, int month, int day, const String &countryCode, String &holidayText)
{
    holidayText = "";
    String code = countryCode;
    code.trim();
    code.toUpperCase();
    if (code.length() != 2)
    {
        return false;
    }

    auto fetchHolidayYear = [&](int queryYear, String &bestText, bool &foundToday, time_t &bestWhen) -> bool
    {
        const String url = String("https://date.nager.at/api/v3/PublicHolidays/") + String(queryYear) + "/" + code;
        String payload;
        NetWorkerHttpOptions net_opts;
        net_opts.insecure_tls = true;
        net_opts.connect_timeout_ms = 5000;
        net_opts.timeout_ms = 6000;
        const int httpCode = net_worker_http_get(url, &payload, &net_opts);
        if (httpCode != HTTP_CODE_OK)
        {
            return false;
        }

        DynamicJsonDocument filter(256);
        filter[0]["types"][0] = true;
        filter[0]["date"] = true;
        filter[0]["name"] = true;
        filter[0]["localName"] = true;

        DynamicJsonDocument doc(6144);
        const DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
        if (err || !doc.is<JsonArray>())
        {
            return false;
        }

        JsonArray holidays = doc.as<JsonArray>();
        const time_t today = make_local_noon(year, month, day);
        for (JsonObject item : holidays)
        {
            JsonArray types = item["types"].as<JsonArray>();
            bool isPublic = types.isNull();
            for (JsonVariant type : types)
            {
                if (String(type.as<const char *>()) == "Public")
                {
                    isPublic = true;
                    break;
                }
            }
            if (!isPublic)
            {
                continue;
            }

            String date = item["date"].as<String>();
            if (date.length() != 10)
            {
                continue;
            }
            const int itemYear = date.substring(0, 4).toInt();
            const int itemMonth = date.substring(5, 7).toInt();
            const int itemDay = date.substring(8, 10).toInt();
            time_t when = make_local_noon(itemYear, itemMonth, itemDay);
            if (when < today)
            {
                continue;
            }

            String name = item["name"].as<String>();
            if (name.length() == 0)
            {
                name = item["localName"].as<String>();
            }
            if (name.length() == 0)
            {
                continue;
            }

            if (same_local_date(when, year, month, day))
            {
                bestText = String("Today: ") + name;
                foundToday = true;
                bestWhen = when;
                return true;
            }

            if (bestWhen == 0 || when < bestWhen)
            {
                bestWhen = when;
                bestText = String("Next: ") + name;
            }
        }

        return true;
    };

    bool foundToday = false;
    time_t bestWhen = 0;
    String bestText = "";
    const bool okThisYear = fetchHolidayYear(year, bestText, foundToday, bestWhen);
    bool okNextYear = false;
    if (!foundToday && bestText.length() == 0 && month == 12 && day >= 20)
    {
        okNextYear = fetchHolidayYear(year + 1, bestText, foundToday, bestWhen);
    }

    holidayText = bestText;
    return okThisYear || okNextYear;
}

static void refresh_holiday_text(int year, int month, int day)
{
    static String s_last_country_code = "";
    static int s_last_year = -1;
    static int s_last_month = -1;
    static int s_last_day = -1;

    if (year < 2024 || month < 1 || month > 12 || day < 1 || day > 31)
    {
        return;
    }

    String countryCode = g_country_code;
    countryCode.trim();
    countryCode.toUpperCase();
    if (s_last_year == year && s_last_month == month && s_last_day == day && s_last_country_code == countryCode)
    {
        return;
    }

    // Keep holiday logic local-only to avoid TLS heap spikes and reboot loops.
    String holidayText = get_religious_holiday_text(year, month, day);

    g_holiday_text = holidayText;
    g_clock_dirty = true;
    s_last_year = year;
    s_last_month = month;
    s_last_day = day;
    s_last_country_code = countryCode;
}
static void ensure_panel_link_ap()
{
    // Hard-disable AP mode to prevent hostap_init / esp-sha crash loops on main board.
    (void)kPanelLinkApEnabled;
    g_panel_link_ap_started = false;
    if (WiFi.getMode() != WIFI_STA)
    {
        WiFi.mode(WIFI_STA);
        delay(20);
    }
    WiFi.setSleep(false);
}

static String current_home_state_text()
{
    String state = sd_vendor_ready() ? "SD ready" : "SD pending";
    state += g_mic_ready ? "  |  mic donor online" : "  |  mic donor offline";
    return state;
}

static void update_home_launch_buttons()
{
    if (!g_home_voice_btn || !g_home_off_btn)
    {
        return;
    }

    const bool systemOn = !g_system_soft_off;
    const lv_color_t idleBg = lv_color_hex(0x162231);
    const lv_color_t idleBorder = lv_color_hex(0x5E86A8);

    if (g_home_radio_btn)
    {
        lv_obj_set_style_bg_color(g_home_radio_btn, systemOn ? idleBg : lv_color_hex(0x101720), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(g_home_radio_btn, systemOn ? idleBorder : lv_color_hex(0x314353), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(g_home_radio_btn, systemOn ? LV_OPA_COVER : LV_OPA_60, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_home_player_btn)
    {
        lv_obj_set_style_bg_color(g_home_player_btn, systemOn ? idleBg : lv_color_hex(0x101720), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(g_home_player_btn, systemOn ? idleBorder : lv_color_hex(0x314353), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(g_home_player_btn, systemOn ? LV_OPA_COVER : LV_OPA_60, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_obj_set_style_bg_color(g_home_voice_btn, systemOn ? idleBg : lv_color_hex(0x18283A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_home_voice_btn, systemOn ? idleBorder : lv_color_hex(0x92BCDD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(g_home_voice_btn, g_system_soft_off ? 10 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(g_home_voice_btn, lv_color_hex(0x7DB2E9), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(g_home_off_btn, g_system_soft_off ? lv_color_hex(0x463027) : idleBg, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_home_off_btn, g_system_soft_off ? lv_color_hex(0xFFD49A) : idleBorder, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(g_home_off_btn, g_system_soft_off ? 12 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(g_home_off_btn, lv_color_hex(0xFFC27A), LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void update_home_labels()
{
    if (g_home_state_label)
    {
        lv_label_set_text(g_home_state_label, "");
    }
    if (g_home_hint_label)
    {
        lv_label_set_text(g_home_hint_label, "");
    }
    if (g_home_wifi_label)
    {
        lv_label_set_text(g_home_wifi_label, "");
    }
    update_home_launch_buttons();
}

static void update_settings_labels()
{
    if (g_settings_title_label)
    {
        lv_label_set_text(g_settings_title_label, "Settings");
    }
    if (g_settings_voice_label)
    {
        lv_label_set_text(g_settings_voice_label, voice_command_language_label());
        _ui_flag_modify(g_settings_voice_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        lv_obj_set_pos(g_settings_voice_label, -1000, -1000);
    }
    if (g_settings_voice_value_label)
    {
        lv_label_set_text(g_settings_voice_value_label, voice_command_language_value_label());
        _ui_flag_modify(g_settings_voice_value_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        lv_obj_set_pos(g_settings_voice_value_label, -1000, -1000);
    }
    if (g_settings_voice_hitbox)
    {
        _ui_flag_modify(g_settings_voice_hitbox, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        lv_obj_clear_flag(g_settings_voice_hitbox, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(g_settings_voice_hitbox, -1000, -1000);
    }
    if (g_settings_voice_cover)
    {
        _ui_flag_modify(g_settings_voice_cover, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_settings_row1_label)
    {
        lv_label_set_text(g_settings_row1_label, "Volume");
    }
    if (g_settings_row2_label)
    {
        lv_label_set_text(g_settings_row2_label, "Backlight");
    }

    _ui_flag_modify(ui_Image16, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Image17, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    lv_obj_clear_flag(ui_Image16, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Image17, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(ui_Image16, -1000, -1000);
    lv_obj_set_pos(ui_Image17, -1000, -1000);
}

static void update_radio_overlay()
{
    if (g_radio_title_label)
    {
        lv_label_set_text(g_radio_title_label, "");
        _ui_flag_modify(g_radio_title_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_radio_status_label)
    {
        lv_label_set_text(g_radio_status_label, "");
        _ui_flag_modify(g_radio_status_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
}

static void update_ai_labels()
{
    if (g_ai_header_icon)
    {
        _ui_flag_modify(g_ai_header_icon, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
    if (g_ai_title_label)
    {
        lv_label_set_text(g_ai_title_label, "Sound modes");
        lv_obj_set_width(g_ai_title_label, 320);
        lv_obj_set_style_text_align(g_ai_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        _ui_flag_modify(g_ai_title_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
    if (g_ai_card_title_1)
    {
        _ui_flag_modify(g_ai_card_title_1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_ai_card_sub_1)
    {
        _ui_flag_modify(g_ai_card_sub_1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_ai_card_title_2)
    {
        _ui_flag_modify(g_ai_card_title_2, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_ai_card_sub_2)
    {
        _ui_flag_modify(g_ai_card_sub_2, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_ai_card_title_3)
    {
        _ui_flag_modify(g_ai_card_title_3, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_ai_card_sub_3)
    {
        _ui_flag_modify(g_ai_card_sub_3, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_ai_card_title_4)
    {
        _ui_flag_modify(g_ai_card_title_4, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_ai_card_sub_4)
    {
        _ui_flag_modify(g_ai_card_sub_4, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }

    const lv_color_t activeColor = lv_color_hex(0x95FFF0);
    const lv_color_t iconBaseColor = lv_color_hex(0xF4F8FF);
    lv_obj_t *icons[] = {ui_Image64, ui_Image65, ui_Image66};
    for (uint8_t i = 0; i < 3; ++i)
    {
        if (!icons[i])
        {
            continue;
        }
        lv_color_t color = iconBaseColor;
        if (i == 0 && g_sound_mode_self_enabled)
        {
            color = activeColor;
        }
        else if (i == 1 && g_sound_mode_ask_enabled)
        {
            color = activeColor;
        }
        else if (i == 2 && g_sound_mode_murmur_enabled)
        {
            color = activeColor;
        }
        lv_obj_set_style_img_recolor_opa(icons[i], LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(icons[i], color, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (ui_Button6)
    {
        _ui_flag_modify(ui_Button6, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (ui_Image67)
    {
        _ui_flag_modify(ui_Image67, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_ai_card_feedback_4)
    {
        _ui_flag_modify(g_ai_card_feedback_4, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
    if (g_ai_mic_timer_arc)
    {
        _ui_flag_modify(g_ai_mic_timer_arc, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }

    const char *statusText = g_system_soft_off ? "System standby" : "Select modes for random play";
    lv_color_t statusColor = lv_color_hex(0xDDE8FF);
    if (g_pending_assistant_action != ASSISTANT_ACTION_NONE)
    {
        statusText = assistant_pending_status(g_pending_assistant_action);
        statusColor = lv_color_hex(0xDFF6FF);
    }
    else if (sd_vendor_is_busy())
    {
        statusText = sd_vendor_status();
        statusColor = lv_color_hex(0xBEE7FF);
    }

    if (g_ai_status_label)
    {
        lv_label_set_text(g_ai_status_label, statusText);
        lv_obj_set_style_text_color(g_ai_status_label, statusColor, LV_PART_MAIN | LV_STATE_DEFAULT);
        _ui_flag_modify(g_ai_status_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }

    refresh_assistant_card_feedbacks();
}
static void configure_home_weather_labels()
{
    constexpr lv_coord_t kHomeLiftY = -40;

    if (!g_temp_degree_dot)
    {
        g_temp_degree_dot = lv_obj_create(ui_Screen1);
        lv_obj_remove_style_all(g_temp_degree_dot);
        lv_obj_set_size(g_temp_degree_dot, 6, 6);
        lv_obj_set_align(g_temp_degree_dot, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(g_temp_degree_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_temp_degree_dot, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_temp_degree_dot, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // Weather widgets are visual-only: keep them transparent for touch so screen flip controls still work.
    lv_obj_clear_flag(ui_Image2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Image6, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Image7, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Image8, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Label2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Label4, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Label5, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Label6, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Label15, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Label16, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Label17, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_temp_degree_dot, LV_OBJ_FLAG_CLICKABLE);
    _ui_flag_modify(ui_Label2, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Label4, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Label5, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Label6, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);

    lv_obj_set_x(ui_Image2, 61);
    lv_obj_set_y(ui_Image2, static_cast<lv_coord_t>(-96 + kHomeLiftY));
    // Keep a safe default icon scale before runtime weather frame is applied.
    lv_img_set_zoom(ui_Image2, 256);
    lv_obj_set_width(ui_Label2, LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui_Label2, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ui_Label2, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_x(ui_Label2, -133);
    lv_obj_set_y(ui_Label2, static_cast<lv_coord_t>(-77 + kHomeLiftY));
    lv_obj_set_align(ui_Label2, LV_ALIGN_CENTER);

    lv_obj_set_y(ui_Image6, static_cast<lv_coord_t>(102 + kHomeLiftY));
    lv_obj_set_y(ui_Image7, static_cast<lv_coord_t>(102 + kHomeLiftY));
    lv_obj_set_y(ui_Image8, static_cast<lv_coord_t>(102 + kHomeLiftY));
    lv_img_set_zoom(ui_Image6, 218); // 85% of 256
    lv_img_set_zoom(ui_Image7, 218); // 85% of 256
    lv_img_set_zoom(ui_Image8, 218); // 85% of 256

    lv_obj_set_style_text_font(ui_Label4, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label5, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label6, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label4, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label5, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label6, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label15, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label16, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label17, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_Label15, "C");
    lv_obj_set_style_text_font(ui_Label15, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_Label16, "m/s");
    lv_obj_set_style_text_font(ui_Label16, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_Label17, "%");
    lv_obj_set_style_text_font(ui_Label17, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_width(ui_Label4, 44);
    lv_obj_set_style_text_align(ui_Label4, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_x(ui_Label4, -164);
    lv_obj_set_y(ui_Label4, static_cast<lv_coord_t>(108 + kHomeLiftY));
    lv_obj_set_align(ui_Label4, LV_ALIGN_CENTER);
    lv_obj_align_to(ui_Label15, ui_Label4, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_align_to(g_temp_degree_dot, ui_Label15, LV_ALIGN_OUT_LEFT_TOP, 0, 3);

    lv_obj_set_width(ui_Label5, 24);
    lv_obj_set_style_text_align(ui_Label5, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_x(ui_Label5, -76);
    lv_obj_set_y(ui_Label5, static_cast<lv_coord_t>(108 + kHomeLiftY));
    lv_obj_set_align(ui_Label5, LV_ALIGN_CENTER);
    lv_obj_set_width(ui_Label16, LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui_Label16, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(ui_Label16, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align_to(ui_Label16, ui_Label5, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    lv_obj_set_width(ui_Label6, 34);
    lv_obj_set_style_text_align(ui_Label6, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_x(ui_Label6, 38);
    lv_obj_set_y(ui_Label6, static_cast<lv_coord_t>(108 + kHomeLiftY));
    lv_obj_set_align(ui_Label6, LV_ALIGN_CENTER);
    lv_obj_set_width(ui_Label17, LV_SIZE_CONTENT);
    lv_label_set_long_mode(ui_Label17, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(ui_Label17, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align_to(ui_Label17, ui_Label6, LV_ALIGN_OUT_RIGHT_MID, 2, 0);

    lv_obj_set_width(ui_Label3, 220);
    lv_label_set_long_mode(ui_Label3, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(ui_Label3, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align_to(ui_Label3, ui_Label1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

    if (g_weather_ready)
    {
        _ui_flag_modify(ui_Image6, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Image7, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Image8, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Label15, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Label16, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Label17, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(g_temp_degree_dot, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
    else
    {
        lv_label_set_text(ui_Label2, g_weather_summary.length() > 0 ? g_weather_summary.c_str() : "No internet");
        lv_label_set_text(ui_Label4, g_weather_temp_text.length() > 0 ? g_weather_temp_text.c_str() : "--");
        lv_label_set_text(ui_Label5, g_weather_wind_text.length() > 0 ? g_weather_wind_text.c_str() : "--");
        lv_label_set_text(ui_Label6, g_weather_humidity_text.length() > 0 ? g_weather_humidity_text.c_str() : "--");
        _ui_flag_modify(ui_Image6, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Image7, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Image8, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Label15, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Label16, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(ui_Label17, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        _ui_flag_modify(g_temp_degree_dot, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
}

static void update_weather_icon(bool reset_frame)
{
    if (!ui_Image2)
    {
        return;
    }
    if (!g_weather_ready)
    {
        _ui_flag_modify(ui_Image2, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        weather_visuals_apply(ui_Image2, &g_weather_icon_frame, true, 3, true);
        return;
    }
    _ui_flag_modify(ui_Image2, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    weather_visuals_apply(ui_Image2, &g_weather_icon_frame, reset_frame, g_weather_code, g_weather_is_day);
}

static void update_home_background()
{
    if (!ui_Image1)
    {
        return;
    }

    if (currentTime_year < 2024)
    {
        // Until time sync completes, keep visible default background.
        lv_img_set_src(ui_Image1, &ui_img_s1_back1_png);
        g_home_bg_applied_key = "__stock_pre_ntp__";
        _ui_flag_modify(ui_Image1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        return;
    }

    const String key = home_bg_sd_assets_key_for(currentTime_mouth, currentTime_hour);
    if (key == g_home_bg_applied_key && !lv_obj_has_flag(ui_Image1, LV_OBJ_FLAG_HIDDEN))
    {
        return;
    }

    if (key.length() == 0)
    {
        lv_img_set_src(ui_Image1, &ui_img_s1_back1_png);
        g_home_bg_applied_key = key;
    }
    else if (home_bg_sd_assets_apply(ui_Image1, currentTime_mouth, currentTime_hour))
    {
        g_home_bg_applied_key = key;
    }
    else
    {
        lv_img_set_src(ui_Image1, &ui_img_s1_back1_png);
        g_home_bg_applied_key = "__stock__";
    }
    _ui_flag_modify(ui_Image1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
}

static void weather_icon_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (lv_scr_act() != ui_Screen1)
    {
        return;
    }
    if (!weather_visuals_is_animated(g_weather_code, g_weather_is_day))
    {
        return;
    }
    update_weather_icon(false);
}

static void ensure_weather_task_running()
{
    if (g_weather_task_handle != NULL)
    {
        const eTaskState state = eTaskGetState(g_weather_task_handle);
        if (state != eDeleted)
        {
            g_weather_task_started = true;
            return;
        }
        g_weather_task_handle = NULL;
        g_weather_task_started = false;
    }

    const uint32_t freeHeapBefore = ESP.getFreeHeap();
    if (xTaskCreatePinnedToCore(getWeather, "getWeather", kWeatherTaskStackBytes, NULL, 1, &g_weather_task_handle, 1) == pdPASS)
    {
        g_weather_task_started = true;
        Serial.printf("[weather] task started stack=%lu heap=%lu\n",
                      static_cast<unsigned long>(kWeatherTaskStackBytes),
                      static_cast<unsigned long>(freeHeapBefore));
    }
    else
    {
        g_weather_task_started = false;
        Serial.printf("[weather] task start failed stack=%lu heap=%lu\n",
                      static_cast<unsigned long>(kWeatherTaskStackBytes),
                      static_cast<unsigned long>(freeHeapBefore));
    }
}

static bool start_wifi_connect_task()
{
    if (g_wifi_connect_state == 1)
    {
        return true;
    }

    g_wifi_connect_state = 1;
    const BaseType_t task_ok = xTaskCreatePinnedToCore(connect_wifi_task, "connect_wifi_task", 4096, NULL, 0, NULL, 1);
    if (task_ok != pdPASS)
    {
        g_wifi_connect_state = 0;
        return false;
    }
    return true;
}

static void update_all_shell_labels()
{
    update_home_labels();
    update_settings_labels();
    update_radio_overlay();
    update_ai_labels();
    update_alarm_labels();
    configure_home_weather_labels();
}

void ui_sync_runtime()
{
    static uint32_t s_panel_link_heal_after_ms = 0;
    static uint32_t s_next_media_sync_ms = 0;
    static uint32_t s_wifi_reconnect_after_ms = 0;
    static uint32_t s_weather_bootstrap_after_ms = 0;
    static bool s_prev_local_audio_active = false;

    const uint32_t now_ms = millis();
    if (kDonorSoundWakeEnabled) {
        mic_remote_client_loop();
    }
    if (clock_time_valid())
    {
        const int minute_key_now = static_cast<int>(currentTime_hour) * 60 + static_cast<int>(currentTime_minute);
        if (minute_key_now != g_last_clock_tick_minute_key)
        {
            const bool clock_screen_open = (lv_scr_act() == ui_Screen13);
            const bool minute_click_time_window = (currentTime_hour >= 9U) && (currentTime_hour < 21U);
            if (clock_screen_open && minute_click_time_window)
            {
                (void)speaker_tone_click();
            }
            g_last_clock_tick_minute_key = minute_key_now;
        }
    }
    audio_visualizer_update_ui();
    service_media_radio_action();
    service_media_sd_scan_task();
    if (now_ms >= s_next_media_sync_ms)
    {
        if (lv_scr_act() == g_media_screen || media_sd_playing() || media_radio_playing())
        {
            update_media_screen_info();
        }
        s_next_media_sync_ms = now_ms + 220UL;
    }

    const bool localRadioActive = media_radio_playing();
    const bool localSdTrackActive = (sd_vendor_music_index() >= 0) && sd_vendor_is_busy();
    const bool localAudioActive = (localRadioActive || localSdTrackActive);
    {
        uint8_t donorVu[8] = {0};
        bool haveVu = audio_visualizer_copy_levels(donorVu, 8);
        if (localAudioActive && !haveVu)
        {
            // Fallback animation so donor EQ still reacts when backend callback is silent.
            const uint8_t base = 18U + static_cast<uint8_t>((millis() / 45U) % 24U);
            donorVu[0] = base;
            donorVu[1] = base + 6U;
            donorVu[2] = base + 12U;
            donorVu[3] = base + 18U;
            donorVu[4] = base + 18U;
            donorVu[5] = base + 12U;
            donorVu[6] = base + 6U;
            donorVu[7] = base;
            haveVu = true;
        }
        const uint32_t nowVu = millis();
        if (localAudioActive && haveVu)
        {
            donor_eq_uart_push(donorVu, 8, true);
            s_last_donor_uart_push_ms = nowVu;
        }
        else if (s_prev_local_audio_active || (nowVu - s_last_donor_uart_push_ms) >= 600)
        {
            uint8_t zeros[8] = {0};
            donor_eq_uart_push(zeros, 8, false);
            s_last_donor_uart_push_ms = nowVu;
        }
        s_prev_local_audio_active = localAudioActive;
    }

    // Mic feature disabled for stability/performance in this build.
    if (!kMicFeatureEnabled)
    {
        g_ai_voice_enabled = false;
        g_mic_ready = false;
        if (g_tap_listen_active || g_tap_listen_transcribing)
        {
            stop_tap_listen();
        }
        g_tap_listen_ui_dirty = false;
        g_tap_listen_task_done = false;
        g_tap_listen_task_error = false;
        g_tap_listen_task_command = VOICE_COMMAND_NONE;
        g_tap_listen_task_text[0] = '\0';
        g_tap_listen_result_text[0] = '\0';
        g_tap_listen_result_until_ms = 0;
        refresh_display_sleep_state();
        s_panel_link_heal_after_ms = 0;
    }


    if (g_pending_assistant_action != ASSISTANT_ACTION_NONE && millis() >= g_pending_assistant_action_at)
    {
        const AssistantActionKind action = g_pending_assistant_action;
        g_pending_assistant_action = ASSISTANT_ACTION_NONE;
        g_pending_assistant_action_at = 0;

        switch (action)
        {
            case ASSISTANT_ACTION_SELF:
                play_sd_action("self");
                break;
            case ASSISTANT_ACTION_ASK:
                play_sd_action("question");
                break;
            case ASSISTANT_ACTION_MURMUR:
                play_sd_action("murmur");
                break;
            case ASSISTANT_ACTION_MIC:
                note_user_activity();
                g_ai_voice_enabled = false;
                stop_tap_listen();
                set_tap_listen_result("Mic disabled", false, 1800);
                update_ai_labels();
                break;
            default:
                break;
        }

        clear_assistant_card_feedback();
        g_pending_assistant_card = 0xFF;
        set_assistant_card_feedback(3, false);
    }

    if (g_boot_wait_wifi_on_start && g_wifi_connect_state == 1)
    {
        show_boot_black_screen();
    }

    if (g_wifi_connect_state == 2)
    {
        g_boot_wait_wifi_on_start = false;
        g_boot_black_active = false;
        save_wifi_credentials();
        lv_label_set_text(ui_Label12, wifi_ui_status_connect_success());
        _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_Screen1_screen_init);
        update_all_shell_labels();
        if (!g_time_task_started)
        {
            xTaskCreatePinnedToCore(getNtpTime, "getNtpTime", 4096, NULL, 1, NULL, 1);
            g_time_task_started = true;
        }
        ensure_weather_task_running();
        g_wifi_connect_state = 0;
    }
    else if (g_wifi_connect_state == 3)
    {
        g_boot_wait_wifi_on_start = false;
        if (g_boot_black_active)
        {
            _ui_screen_change(&ui_Screen2, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen2_screen_init);
            g_boot_black_active = false;
        }
        g_weather_ready = false;
        g_weather_dirty = true;
        lv_label_set_text(ui_Label12, wifi_ui_status_connect_failed());
        lv_obj_clear_flag(ui_Button1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_Button2, LV_OBJ_FLAG_HIDDEN);
        g_wifi_connect_state = 0;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        s_wifi_reconnect_after_ms = 0;
    }
    else if (g_wifi_connect_state == 0 && ssid.length() > 0)
    {
        if (s_wifi_reconnect_after_ms == 0 || now_ms >= s_wifi_reconnect_after_ms)
        {
            if (!start_wifi_connect_task())
            {
                s_wifi_reconnect_after_ms = now_ms + 8000UL;
            }
            else
            {
                s_wifi_reconnect_after_ms = now_ms + 15000UL;
            }
        }
    }

    if (WiFi.status() == WL_CONNECTED && !g_weather_ready)
    {
        ensure_weather_task_running();
        if (g_weather_summary.length() == 0 || g_weather_summary == "No internet")
        {
            g_weather_summary = "Weather loading";
            g_weather_dirty = true;
        }
        if (s_weather_bootstrap_after_ms == 0 || now_ms >= s_weather_bootstrap_after_ms)
        {
            const bool bootstrapOk = fetch_weather_once_now();
            Serial.printf("[weather] bootstrap %s\n", bootstrapOk ? "ok" : "fail");
            s_weather_bootstrap_after_ms = now_ms + 15000UL;
        }
    }
    else
    {
        // Reset retry cadence once weather is available or Wi-Fi is down.
        s_weather_bootstrap_after_ms = 0;
    }

    alarm_check_and_handle();

    if (g_clock_dirty)
    {
        lv_obj_set_y(ui_Label1, -40);
        lv_obj_set_align(ui_Label1, LV_ALIGN_CENTER);
        lv_label_set_text(ui_Label1, topTimeText.c_str());
        lv_label_set_text(ui_Label3, topDateText.c_str());
        lv_obj_align_to(ui_Label3, ui_Label1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
        update_home_background();
        g_clock_dirty = false;
    }

    if (lv_scr_act() == ui_Screen13 || g_clock_dirty)
    {
        flip_clock_set_datetime(currentTime_hour, currentTime_minute, currentTime_second,
                                weekText.c_str(), topDateText.c_str(),
                                g_holiday_text.c_str(), g_clock_timezone_text.c_str());
    }

    if (g_weather_dirty)
    {
        configure_home_weather_labels();
        if (g_weather_ready)
        {
            lv_label_set_text(ui_Label2, g_weather_summary.c_str());
            lv_label_set_text(ui_Label4, g_weather_temp_text.c_str());
            lv_label_set_text(ui_Label5, g_weather_wind_text.c_str());
            lv_label_set_text(ui_Label6, g_weather_humidity_text.c_str());
        }
        else
        {
            lv_label_set_text(ui_Label2, g_weather_summary.length() > 0 ? g_weather_summary.c_str() : "No internet");
            lv_label_set_text(ui_Label4, g_weather_temp_text.length() > 0 ? g_weather_temp_text.c_str() : "--");
            lv_label_set_text(ui_Label5, g_weather_wind_text.length() > 0 ? g_weather_wind_text.c_str() : "--");
            lv_label_set_text(ui_Label6, g_weather_humidity_text.length() > 0 ? g_weather_humidity_text.c_str() : "--");
        }
        update_weather_icon(true);
        g_weather_dirty = false;
    }
    if (!g_system_soft_off && sd_vendor_ready() && !sd_vendor_is_busy() && !radio_vendor_is_playing() &&
        !weather_sd_assets_session_active() &&
        g_next_ambient_ms != 0 && millis() >= g_next_ambient_ms && current_screen_allows_ambient())
    {
        const bool ok = play_mic_reactive_response(0);
        if (ok)
        {
            update_ai_labels();
        }
        schedule_next_ambient(false);
    }
}

static void show_assistant_status(const char *message)
{
    if (!g_ai_status_label)
    {
        return;
    }

    lv_label_set_text(g_ai_status_label, (message && message[0]) ? message : "");
    lv_obj_set_style_text_color(g_ai_status_label, lv_color_hex(0xDFF6FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    _ui_flag_modify(g_ai_status_label, LV_OBJ_FLAG_HIDDEN, (message && message[0]) ? _UI_MODIFY_FLAG_REMOVE : _UI_MODIFY_FLAG_ADD);
}
static void schedule_next_ambient(bool initial)
{
    const uint32_t now = millis();
    const uint32_t min_gap = initial ? 120000UL : 360000UL;
    const uint32_t max_gap = initial ? 240000UL : 780000UL;
    g_next_ambient_ms = now + (uint32_t)random((long)min_gap, (long)max_gap);
}

static void apply_runtime_backlight(uint8_t level)
{
    ledcWrite(0, level);
}

static bool media_output_active()
{
    return radio_vendor_is_playing() || sd_vendor_is_busy();
}

static bool mic_sound_above_wake_level()
{
    WakeLogicInputs in;
    in.donor_wake_enabled = kDonorWakeGpioEnabled;
    in.donor_gpio_high = (digitalRead(kDonorWakeGpioInPin) == HIGH);
    in.now_ms = millis();
    in.donor_wake_hold_ms = kDonorWakeHoldMs;
    in.system_soft_off = g_system_soft_off;
    in.media_output_active = media_output_active();
    in.donor_sound_wake_enabled = kDonorSoundWakeEnabled;
    in.mic_online = mic_remote_client_online();
    in.mic_voice = mic_remote_client_voice();
    in.display_sleeping = g_display_sleeping;
    in.noise_floor = mic_remote_client_noise_floor();
    in.gate_on = mic_remote_client_gate_on();
    in.rms_raw = mic_remote_client_rms();
    in.rms_smooth = mic_remote_client_rms_smooth();
    in.peak = mic_remote_client_peak();
    return wake_logic_update(in);
}

static void refresh_display_sleep_state()
{
    const int16_t level = power_manager_refresh(
        millis(),
        &g_last_user_activity_ms,
        &g_last_sound_activity_ms,
        &g_display_wake_hold_until_ms,
        &g_display_sleeping,
        g_system_soft_off,
        g_alarm_ringing,
        mic_sound_above_wake_level(),
        app_settings::kDisplaySleepIdleMs,
        app_settings::kDisplaySleepWakeHoldMs,
        g_backlight,
        app_settings::kDisplaySleepBrightness,
        app_settings::kSystemSoftOffBrightness);
    if (level >= 0)
    {
        apply_runtime_backlight(static_cast<uint8_t>(level));
    }
}

static void note_user_activity()
{
    const int16_t level = power_manager_note_user_activity(
        millis(),
        g_system_soft_off,
        &g_last_user_activity_ms,
        &g_display_sleeping,
        g_backlight);
    if (level >= 0)
    {
        apply_runtime_backlight(static_cast<uint8_t>(level));
    }
    schedule_next_ambient(false);
}

static bool current_screen_allows_ambient()
{
    lv_obj_t *screen = lv_scr_act();
    return screen == ui_Screen1 || screen == ui_Screen13;
}

static bool play_sd_action(const char *kind)
{
    if (g_system_soft_off)
    {
        show_assistant_status("System off");
        return false;
    }
    if (!sd_vendor_ready())
    {
        show_assistant_status("SD pack missing or not mounted.");
        return false;
    }

    radio_ctrl_stop();

    const SdVoiceMode mode = current_voice_mode();
    bool ok = false;
    if (strcmp(kind, "self") == 0)
    {
        ok = sd_vendor_play_selftalk_mode(mode);
        show_assistant_status(ok ? "Self" : sd_vendor_status());
    }
    else if (strcmp(kind, "question") == 0)
    {
        ok = sd_vendor_play_question_mode(mode);
        show_assistant_status(ok ? "Question" : sd_vendor_status());
    }
    else if (strcmp(kind, "murmur") == 0)
    {
        ok = sd_vendor_play_murmur_mode(mode);
        show_assistant_status(ok ? "Murmur" : sd_vendor_status());
    }

    if (ok)
    {
        note_user_activity();
    }
    return ok;
}

static bool play_mic_reactive_response(uint32_t voice_duration_ms)
{
    (void)voice_duration_ms;
    if (!sd_vendor_ready() || sd_vendor_is_busy() || radio_vendor_is_playing())
    {
        return false;
    }

    AssistantActionKind actions[3] = {ASSISTANT_ACTION_NONE, ASSISTANT_ACTION_NONE, ASSISTANT_ACTION_NONE};
    const uint8_t count = collect_enabled_sound_actions(actions, 3);
    if (count == 0)
    {
        show_assistant_status("No sound modes on");
        return false;
    }

    const AssistantActionKind chosen = actions[random((long)count)];
    bool ok = false;
    switch (chosen)
    {
        case ASSISTANT_ACTION_SELF:
            ok = play_sd_action("self");
            break;
        case ASSISTANT_ACTION_ASK:
            ok = play_sd_action("question");
            break;
        case ASSISTANT_ACTION_MURMUR:
            ok = play_sd_action("murmur");
            break;
        default:
            break;
    }

    if (ok)
    {
        note_user_activity();
        set_tap_listen_result("Heard", false, 1500);
    }
    return ok;
}

static void show_wifi_required(const char *message)
{
    lv_label_set_text(ui_Label12, message);
    lv_scr_load(ui_Screen2);
}

static void play_ui_click()
{
    return;
}

static void play_ui_action()
{
    return;
}

static void configure_assistant_card_ui()
{
    lv_obj_t *buttons[] = {ui_Button3, ui_Button4, ui_Button5, ui_Button6};
    lv_obj_t *images[] = {ui_Image64, ui_Image65, ui_Image66, ui_Image67};
    const lv_img_dsc_t *iconSources[] = {
        &ui_img_s4_card1_png,
        &ui_img_s4_card2_png,
        &ui_img_s4_card3_png,
        &ui_img_s4_card4_png,
    };
    const lv_coord_t iconY = kAssistantCardY - 8;

    for (uint8_t i = 0; i < 4; ++i)
    {
        lv_obj_t *button = buttons[i];
        lv_obj_t *image = images[i];
        if (!button || !image)
        {
            continue;
        }

        lv_obj_remove_event_cb(button, ui_event_Button3);
        lv_obj_remove_event_cb(button, ui_event_Button4);
        lv_obj_remove_event_cb(button, ui_event_Button5);
        lv_obj_remove_event_cb(button, ui_event_Button6);
        lv_obj_remove_event_cb(image, ui_event_Image64);
        lv_obj_remove_event_cb(image, ui_event_Image65);
        lv_obj_remove_event_cb(image, ui_event_Image66);
        lv_obj_remove_event_cb(image, ui_event_Image67);

        lv_obj_set_size(button, kAssistantCardWidth, kAssistantCardHeight);
        lv_obj_set_align(button, LV_ALIGN_CENTER);
        lv_obj_set_pos(button, kAssistantCardXs[i], kAssistantCardY);
        lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(button, 76, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(button, 2, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(button, 22, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_outline_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(button, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x76D8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(button, lv_color_hex(0xD7F7FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(button, lv_color_hex(0xF4FCFF), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_shadow_color(button, lv_color_hex(0x63DFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(button, lv_color_hex(0x63DFFF), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_shadow_opa(button, 140, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(button, 200, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_flag(button, LV_OBJ_FLAG_PRESS_LOCK);

        lv_img_set_src(image, iconSources[i]);
        lv_obj_set_align(image, LV_ALIGN_CENTER);
        lv_obj_set_pos(image, kAssistantCardXs[i], iconY);
        lv_img_set_zoom(image, 230);
        _ui_flag_modify(image, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(image, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_move_foreground(image);
        lv_obj_move_foreground(button);

        if (i == 3)
        {
            _ui_flag_modify(button, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
            _ui_flag_modify(image, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        }
    }

    refresh_assistant_card_feedbacks();

    if (g_ai_mic_timer_arc)
    {
        lv_obj_set_pos(g_ai_mic_timer_arc, kAssistantCardXs[3], iconY);
        lv_obj_move_foreground(g_ai_mic_timer_arc);
    }
}
static void queue_assistant_action(AssistantActionKind action)
{
    note_user_activity();
    switch (action)
    {
        case ASSISTANT_ACTION_SELF:
            g_sound_mode_self_enabled = !g_sound_mode_self_enabled;
            break;
        case ASSISTANT_ACTION_ASK:
            g_sound_mode_ask_enabled = !g_sound_mode_ask_enabled;
            break;
        case ASSISTANT_ACTION_MURMUR:
            g_sound_mode_murmur_enabled = !g_sound_mode_murmur_enabled;
            break;
        case ASSISTANT_ACTION_MIC:
            g_ai_voice_enabled = false;
            stop_tap_listen();
            set_tap_listen_result("Mic disabled", false, 1800);
            break;
        default:
            break;
    }

    g_pending_assistant_action = ASSISTANT_ACTION_NONE;
    g_pending_assistant_action_at = 0;
    g_pending_assistant_card = 0xFF;
    refresh_assistant_card_feedbacks();
    show_assistant_status(assistant_pending_status(action));
    update_ai_labels();
    update_home_labels();
}
static void assistant_card_event_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    const AssistantActionKind action = (AssistantActionKind)(uintptr_t)lv_event_get_user_data(e);
    const uint8_t card_index = assistant_card_for_action(action);

    if (card_index == 0xFF)
    {
        return;
    }

    if (code == LV_EVENT_PRESSED)
    {
        note_user_activity();
        set_assistant_card_feedback(card_index, true);
        return;
    }

    if (code == LV_EVENT_PRESS_LOST)
    {
        if (!assistant_card_enabled(card_index))
        {
            set_assistant_card_feedback(card_index, false);
        }
        return;
    }

    if (code == LV_EVENT_CLICKED)
    {
        queue_assistant_action(action);
    }
}
static void init_overlay_ui()
{
    if (!g_home_wifi_label)
    {
        g_home_wifi_label = create_overlay_label(ui_Screen1, 180, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_RIGHT);
        lv_obj_align(g_home_wifi_label, LV_ALIGN_TOP_RIGHT, -20, 18);
        g_home_state_label = create_overlay_label(ui_Screen1, 360, &ui_font_Font4, lv_color_hex(0xDDE8FF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_home_state_label, LV_ALIGN_BOTTOM_MID, 0, -56);
        g_home_hint_label = create_overlay_label(ui_Screen1, 400, &ui_font_Font4, lv_color_hex(0xA9B8D5), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_home_hint_label, LV_ALIGN_BOTTOM_MID, 0, -26);
    }


    if (!g_settings_title_label)
    {
        g_settings_title_label = create_overlay_label(ui_Screen10, 180, &ui_font_Font2, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_settings_title_label, LV_ALIGN_TOP_MID, 0, 18);
        g_settings_voice_label = create_overlay_label(ui_Screen10, 170, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_settings_voice_label, LV_ALIGN_CENTER, -96, -166);
        lv_obj_move_foreground(g_settings_voice_label);
        g_settings_voice_value_label = create_overlay_label(ui_Screen10, 170, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_settings_voice_value_label, LV_ALIGN_CENTER, -96, -116);
        lv_obj_move_foreground(g_settings_voice_value_label);
        g_settings_row1_label = create_overlay_label(ui_Screen10, 180, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_LEFT);
        lv_obj_align(g_settings_row1_label, LV_ALIGN_CENTER, 0, -52);
        g_settings_row2_label = create_overlay_label(ui_Screen10, 180, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_LEFT);
        lv_obj_align(g_settings_row2_label, LV_ALIGN_CENTER, 0, 58);

        g_settings_voice_hitbox = lv_obj_create(ui_Screen10);
        lv_obj_remove_style_all(g_settings_voice_hitbox);
        lv_obj_set_size(g_settings_voice_hitbox, 176, 52);
        lv_obj_align(g_settings_voice_hitbox, LV_ALIGN_CENTER, -96, -116);
        lv_obj_add_flag(g_settings_voice_hitbox, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(g_settings_voice_hitbox, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(g_settings_voice_hitbox, LV_OPA_80, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_settings_voice_hitbox, lv_color_hex(0x162231), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(g_settings_voice_hitbox, lv_color_hex(0x5E86A8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_settings_voice_hitbox, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(g_settings_voice_hitbox, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(g_settings_voice_hitbox, settings_voice_language_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_text_color(g_settings_voice_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(g_settings_voice_label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_settings_voice_value_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(g_settings_voice_value_label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(g_settings_voice_label);
        lv_obj_move_foreground(g_settings_voice_value_label);

        g_settings_voice_cover = lv_obj_create(ui_Screen10);
        lv_obj_remove_style_all(g_settings_voice_cover);
        // Fully hide legacy mic graphics baked into the left side of the Settings background.
        lv_obj_set_size(g_settings_voice_cover, 206, 92);
        lv_obj_align(g_settings_voice_cover, LV_ALIGN_CENTER, -98, -141);
        lv_obj_clear_flag(g_settings_voice_cover, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(g_settings_voice_cover, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(g_settings_voice_cover, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_settings_voice_cover, lv_color_hex(0x101826), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(g_settings_voice_cover, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_settings_voice_cover, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        _ui_flag_modify(g_settings_voice_cover, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

        // Settings icons: dedicated GitHub-sourced volume + brightness glyphs.
        const lv_coord_t kSettingsSliderLeftX = -187;
        lv_img_set_src(ui_Image15, &ui_img_settings_volume_png);
        lv_obj_set_x(ui_Image15, kSettingsSliderLeftX);
        lv_obj_set_y(ui_Image15, -52);
        lv_img_set_zoom(ui_Image15, 384); // 1.5x
        lv_obj_set_style_img_recolor(ui_Image15, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_Image15, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        _ui_flag_modify(ui_Image15, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        lv_obj_move_foreground(ui_Image15);

        lv_img_set_src(ui_Image14, &ui_img_settings_brightness_png);
        lv_obj_set_x(ui_Image14, kSettingsSliderLeftX);
        lv_obj_set_y(ui_Image14, 58);
        lv_img_set_zoom(ui_Image14, 384); // 1.5x
        lv_obj_set_style_img_recolor(ui_Image14, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(ui_Image14, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(ui_Image14);

        if (!g_settings_mic_icon_img)
        {
            g_settings_mic_icon_img = lv_img_create(ui_Screen10);
            lv_obj_clear_flag(g_settings_mic_icon_img, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_align(g_settings_mic_icon_img, LV_ALIGN_CENTER);
        }
        lv_img_set_src(g_settings_mic_icon_img, &ui_img_settings_mic_png);
        lv_obj_set_pos(g_settings_mic_icon_img, kSettingsSliderLeftX, -166);
        lv_img_set_zoom(g_settings_mic_icon_img, 384); // 1.5x
        lv_obj_set_style_img_recolor(g_settings_mic_icon_img, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(g_settings_mic_icon_img, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        _ui_flag_modify(g_settings_mic_icon_img, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        lv_obj_move_foreground(g_settings_mic_icon_img);

        _ui_flag_modify(g_settings_voice_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(g_settings_voice_value_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(g_settings_voice_hitbox, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(g_settings_voice_cover, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

        if (g_settings_volume_icon_label)
        {
            _ui_flag_modify(g_settings_volume_icon_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        }
        create_home_shortcut(ui_Screen10);
    }

    if (!g_alarm_state_label)
    {
        g_alarm_toggle_hitbox = lv_obj_create(ui_Screen13);
        lv_obj_remove_style_all(g_alarm_toggle_hitbox);
        lv_obj_set_size(g_alarm_toggle_hitbox, 200, 34);
        lv_obj_align(g_alarm_toggle_hitbox, LV_ALIGN_CENTER, -110, 64);
        lv_obj_add_flag(g_alarm_toggle_hitbox, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(g_alarm_toggle_hitbox, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_clear_flag(g_alarm_toggle_hitbox, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(g_alarm_toggle_hitbox, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_style_bg_opa(g_alarm_toggle_hitbox, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_alarm_toggle_hitbox, lv_color_hex(0x162231), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(g_alarm_toggle_hitbox, lv_color_hex(0x5E86A8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_alarm_toggle_hitbox, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(g_alarm_toggle_hitbox, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(g_alarm_toggle_hitbox, alarm_toggle_event_cb, LV_EVENT_CLICKED, NULL);

        g_alarm_time_hitbox = lv_obj_create(ui_Screen13);
        lv_obj_remove_style_all(g_alarm_time_hitbox);
        lv_obj_set_size(g_alarm_time_hitbox, 200, 34);
        lv_obj_align(g_alarm_time_hitbox, LV_ALIGN_CENTER, 110, 64);
        lv_obj_clear_flag(g_alarm_time_hitbox, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(g_alarm_time_hitbox, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(g_alarm_time_hitbox, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_alarm_time_hitbox, lv_color_hex(0x162231), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(g_alarm_time_hitbox, lv_color_hex(0x5E86A8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_alarm_time_hitbox, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(g_alarm_time_hitbox, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

        g_alarm_minus_hitbox = lv_obj_create(ui_Screen13);
        lv_obj_remove_style_all(g_alarm_minus_hitbox);
        lv_obj_set_size(g_alarm_minus_hitbox, 48, 30);
        lv_obj_align(g_alarm_minus_hitbox, LV_ALIGN_CENTER, 38, 64);
        lv_obj_add_flag(g_alarm_minus_hitbox, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(g_alarm_minus_hitbox, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_clear_flag(g_alarm_minus_hitbox, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(g_alarm_minus_hitbox, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_style_bg_opa(g_alarm_minus_hitbox, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_alarm_minus_hitbox, lv_color_hex(0x2D4864), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_alarm_minus_hitbox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(g_alarm_minus_hitbox, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(g_alarm_minus_hitbox, alarm_minus_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(g_alarm_minus_hitbox, alarm_minus_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);

        g_alarm_plus_hitbox = lv_obj_create(ui_Screen13);
        lv_obj_remove_style_all(g_alarm_plus_hitbox);
        lv_obj_set_size(g_alarm_plus_hitbox, 48, 30);
        lv_obj_align(g_alarm_plus_hitbox, LV_ALIGN_CENTER, 182, 64);
        lv_obj_add_flag(g_alarm_plus_hitbox, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(g_alarm_plus_hitbox, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_clear_flag(g_alarm_plus_hitbox, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(g_alarm_plus_hitbox, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_style_bg_opa(g_alarm_plus_hitbox, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_alarm_plus_hitbox, lv_color_hex(0x2D4864), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_alarm_plus_hitbox, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(g_alarm_plus_hitbox, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(g_alarm_plus_hitbox, alarm_plus_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(g_alarm_plus_hitbox, alarm_plus_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);

        g_alarm_state_label = create_overlay_label(ui_Screen13, 180, &ui_font_Font4, lv_color_hex(0xDDE8FF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_alarm_state_label, LV_ALIGN_CENTER, -110, 66);
        lv_obj_add_flag(g_alarm_state_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(g_alarm_state_label, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_clear_flag(g_alarm_state_label, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(g_alarm_state_label, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(g_alarm_state_label, alarm_toggle_event_cb, LV_EVENT_CLICKED, NULL);

        g_alarm_time_label = create_overlay_label(ui_Screen13, 92, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_alarm_time_label, LV_ALIGN_CENTER, 110, 66);

        g_alarm_minus_label = create_overlay_label(ui_Screen13, 28, &ui_font_Font2, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_label_set_text(g_alarm_minus_label, "-");
        lv_obj_align(g_alarm_minus_label, LV_ALIGN_CENTER, 38, 66);
        lv_obj_add_flag(g_alarm_minus_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(g_alarm_minus_label, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_clear_flag(g_alarm_minus_label, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(g_alarm_minus_label, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(g_alarm_minus_label, alarm_minus_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(g_alarm_minus_label, alarm_minus_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);

        g_alarm_plus_label = create_overlay_label(ui_Screen13, 28, &ui_font_Font2, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_label_set_text(g_alarm_plus_label, "+");
        lv_obj_align(g_alarm_plus_label, LV_ALIGN_CENTER, 182, 66);
        lv_obj_add_flag(g_alarm_plus_label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(g_alarm_plus_label, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_clear_flag(g_alarm_plus_label, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(g_alarm_plus_label, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(g_alarm_plus_label, alarm_plus_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(g_alarm_plus_label, alarm_plus_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);

        const struct MelodySpec
        {
            lv_obj_t **slot;
            lv_coord_t x;
            uint8_t id;
        } melodySpecs[] = {
            {&g_alarm_melody_btn_1, -58, 1},
            {&g_alarm_melody_btn_2, 0, 2},
            {&g_alarm_melody_btn_3, 58, 3},
        };

        for (const MelodySpec &spec : melodySpecs)
        {
            lv_obj_t *btn = lv_obj_create(ui_Screen13);
            lv_obj_remove_style_all(btn);
            lv_obj_set_size(btn, 44, 28);
            lv_obj_align(btn, LV_ALIGN_CENTER, spec.x, 114);
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
            lv_obj_set_style_bg_opa(btn, LV_OPA_50, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2D4864), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x5E86A8), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(btn, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(btn, alarm_melody_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)spec.id);

            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text_fmt(label, "%u", (unsigned)spec.id);
            lv_obj_set_style_text_font(label, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
            lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

            *spec.slot = btn;
        }

        lv_obj_move_foreground(g_alarm_toggle_hitbox);
        lv_obj_move_foreground(g_alarm_time_hitbox);
        lv_obj_move_foreground(g_alarm_minus_hitbox);
        lv_obj_move_foreground(g_alarm_plus_hitbox);
        lv_obj_move_foreground(g_alarm_state_label);
        lv_obj_move_foreground(g_alarm_time_label);
        lv_obj_move_foreground(g_alarm_minus_label);
        lv_obj_move_foreground(g_alarm_plus_label);
        lv_obj_move_foreground(g_alarm_melody_btn_1);
        lv_obj_move_foreground(g_alarm_melody_btn_2);
        lv_obj_move_foreground(g_alarm_melody_btn_3);
        create_home_shortcut(ui_Screen13);
    }
    if (!g_radio_title_label)
    {
        g_radio_title_label = create_overlay_label(ui_Screen11, 220, &ui_font_Font2, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_radio_title_label, LV_ALIGN_TOP_MID, 0, 18);
        g_radio_status_label = create_overlay_label(ui_Screen11, 360, &ui_font_Font4, lv_color_hex(0xDDE8FF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_radio_status_label, LV_ALIGN_TOP_MID, 0, 54);
        _ui_flag_modify(g_radio_title_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(g_radio_status_label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        create_home_shortcut(ui_Screen11);
    }

    if (!g_ai_title_label)
    {
        lv_obj_set_style_bg_color(ui_Screen12, lv_color_hex(0x09111B), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(ui_Screen12, lv_color_hex(0x09111B), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(ui_Screen12, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_Screen12, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *legacyImages[] = {ui_Image39, ui_Image40, ui_Image41, ui_Image42, ui_Image43, ui_Image44, ui_Image45, ui_Image46};
        for (lv_obj_t *legacy : legacyImages)
        {
            if (legacy)
            {
                _ui_flag_modify(legacy, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
            }
        }

        lv_obj_t *top_panel = lv_obj_create(ui_Screen12);
        lv_obj_remove_style_all(top_panel);
        lv_obj_set_size(top_panel, 460, 194);
        lv_obj_align(top_panel, LV_ALIGN_TOP_MID, 0, 4);
        lv_obj_clear_flag(top_panel, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(top_panel, 36, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(top_panel, lv_color_hex(0x121A28), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(top_panel, lv_color_hex(0x121A28), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(top_panel, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(top_panel, 245, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(top_panel, lv_color_hex(0x2A3F5C), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(top_panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(top_panel, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(top_panel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(top_panel, 70, LV_PART_MAIN | LV_STATE_DEFAULT);

        for (uint8_t i = 0; i < 4; ++i)
        {
            lv_obj_t *card_shell = lv_obj_create(ui_Screen12);
            lv_obj_remove_style_all(card_shell);
            lv_obj_set_size(card_shell, kAssistantCardWidth, kAssistantCardHeight);
            lv_obj_set_align(card_shell, LV_ALIGN_CENTER);
            lv_obj_set_pos(card_shell, kAssistantCardXs[i], kAssistantCardY);
            lv_obj_clear_flag(card_shell, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_radius(card_shell, 28, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(card_shell, lv_color_hex(0x141D2B), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_grad_color(card_shell, lv_color_hex(0x141D2B), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_grad_dir(card_shell, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(card_shell, 235, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(card_shell, lv_color_hex(0x24374F), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(card_shell, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(card_shell, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_color(card_shell, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_opa(card_shell, 55, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_t *accent = lv_obj_create(card_shell);
            lv_obj_remove_style_all(accent);
            lv_obj_set_size(accent, 30, 4);
            lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 14);
            lv_obj_clear_flag(accent, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_radius(accent, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(accent, lv_color_hex(0x76AEE0), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(accent, 180, LV_PART_MAIN | LV_STATE_DEFAULT);

            if (i == 3)
            {
                _ui_flag_modify(card_shell, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
            }
        }

        const lv_coord_t dotXs[5] = {-60, -30, 0, 30, 60};
        for (uint8_t i = 0; i < 5; ++i)
        {
            lv_obj_t *dot = lv_obj_create(ui_Screen12);
            lv_obj_remove_style_all(dot);
            lv_obj_set_size(dot, 10, 10);
            lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, dotXs[i], -18);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(dot, (i == 2) ? lv_color_hex(0x75D8FF) : lv_color_hex(0x3A5169), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(dot, (i == 2) ? 255 : 165, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        g_ai_header_icon = lv_img_create(ui_Screen12);
        lv_img_set_src(g_ai_header_icon, &ui_img_sound_modes_322_png);
        lv_img_set_zoom(g_ai_header_icon, 245);
        lv_obj_align(g_ai_header_icon, LV_ALIGN_TOP_MID, 0, 6);
        lv_obj_clear_flag(g_ai_header_icon, LV_OBJ_FLAG_CLICKABLE);

        g_ai_title_label = create_overlay_label(ui_Screen12, 320, &ui_font_Font3, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_title_label, LV_ALIGN_TOP_MID, 0, 138);
        g_ai_status_label = create_overlay_label(ui_Screen12, 390, &ui_font_Font3, lv_color_hex(0xDDE8FF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_status_label, LV_ALIGN_TOP_MID, 0, 162);

        lv_obj_t **feedbacks[] = {
            &g_ai_card_feedback_1,
            &g_ai_card_feedback_2,
            &g_ai_card_feedback_3,
            &g_ai_card_feedback_4,
        };
        for (uint8_t i = 0; i < 4; ++i)
        {
            *feedbacks[i] = lv_obj_create(ui_Screen12);
            lv_obj_remove_style_all(*feedbacks[i]);
            lv_obj_set_size(*feedbacks[i], kAssistantCardWidth, kAssistantCardHeight);
            lv_obj_set_align(*feedbacks[i], LV_ALIGN_CENTER);
            lv_obj_set_pos(*feedbacks[i], kAssistantCardXs[i], kAssistantCardY);
            lv_obj_clear_flag(*feedbacks[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(*feedbacks[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_radius(*feedbacks[i], 28, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(*feedbacks[i], lv_color_hex(0x70D8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(*feedbacks[i], 78, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(*feedbacks[i], lv_color_hex(0xBFF4FF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(*feedbacks[i], 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_outline_width(*feedbacks[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(*feedbacks[i], 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_color(*feedbacks[i], lv_color_hex(0x72E7FF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_opa(*feedbacks[i], 150, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        g_ai_mic_timer_arc = lv_arc_create(ui_Screen12);
        lv_obj_set_size(g_ai_mic_timer_arc, 84, 84);
        lv_obj_set_align(g_ai_mic_timer_arc, LV_ALIGN_CENTER);
        lv_obj_set_pos(g_ai_mic_timer_arc, kAssistantCardXs[3], 82);
        lv_arc_set_rotation(g_ai_mic_timer_arc, 270);
        lv_arc_set_bg_angles(g_ai_mic_timer_arc, 0, 360);
        lv_arc_set_range(g_ai_mic_timer_arc, 0, 100);
        lv_arc_set_value(g_ai_mic_timer_arc, 0);
        lv_obj_remove_style(g_ai_mic_timer_arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(g_ai_mic_timer_arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(g_ai_mic_timer_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_arc_width(g_ai_mic_timer_arc, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_width(g_ai_mic_timer_arc, 6, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(g_ai_mic_timer_arc, lv_color_hex(0x31475D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_opa(g_ai_mic_timer_arc, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(g_ai_mic_timer_arc, lv_color_hex(0xDDE8FF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_opa(g_ai_mic_timer_arc, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_move_foreground(g_ai_mic_timer_arc);

        g_ai_card_title_1 = create_overlay_label(ui_Screen12, 70, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_card_title_1, LV_ALIGN_CENTER, kAssistantCardXs[0], 82);
        g_ai_card_sub_1 = create_overlay_label(ui_Screen12, 70, &ui_font_Font4, lv_color_hex(0xDDE8FF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_card_sub_1, LV_ALIGN_CENTER, kAssistantCardXs[0], 104);

        g_ai_card_title_2 = create_overlay_label(ui_Screen12, 70, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_card_title_2, LV_ALIGN_CENTER, kAssistantCardXs[1], 82);
        g_ai_card_sub_2 = create_overlay_label(ui_Screen12, 70, &ui_font_Font4, lv_color_hex(0xDDE8FF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_card_sub_2, LV_ALIGN_CENTER, kAssistantCardXs[1], 104);

        g_ai_card_title_3 = create_overlay_label(ui_Screen12, 70, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_card_title_3, LV_ALIGN_CENTER, kAssistantCardXs[2], 82);
        g_ai_card_sub_3 = create_overlay_label(ui_Screen12, 70, &ui_font_Font4, lv_color_hex(0xDDE8FF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_card_sub_3, LV_ALIGN_CENTER, kAssistantCardXs[2], 104);

        g_ai_card_title_4 = create_overlay_label(ui_Screen12, 70, &ui_font_Font4, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_card_title_4, LV_ALIGN_CENTER, kAssistantCardXs[3], 82);
        g_ai_card_sub_4 = create_overlay_label(ui_Screen12, 70, &ui_font_Font4, lv_color_hex(0xDDE8FF), LV_TEXT_ALIGN_CENTER);
        lv_obj_align(g_ai_card_sub_4, LV_ALIGN_CENTER, kAssistantCardXs[3], 104);
        _ui_flag_modify(g_ai_card_title_4, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(g_ai_card_sub_4, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

        lv_obj_t *hidden_labels[] = {
            g_ai_title_label,
            g_ai_status_label,
            g_ai_card_title_1,
            g_ai_card_title_2,
            g_ai_card_title_3,
            g_ai_card_title_4,
            g_ai_card_sub_1,
            g_ai_card_sub_2,
            g_ai_card_sub_3,
            g_ai_card_sub_4,
        };
        for (lv_obj_t *label : hidden_labels)
        {
            if (!label)
            {
                continue;
            }
            lv_label_set_text(label, "");
            _ui_flag_modify(label, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        }
        create_home_shortcut(ui_Screen12);
    }

    configure_assistant_card_ui();

    if (!g_weather_icon_timer)
    {
        g_weather_icon_timer = lv_timer_create(weather_icon_timer_cb, 180, NULL);
    }

    update_all_shell_labels();
    update_weather_icon(true);
    enable_home_gesture_bubble();
}

static void handle_assistant_action(const char *message, bool requires_wifi)
{
    if (requires_wifi && WiFi.status() != WL_CONNECTED)
    {
        show_wifi_required("Connect WiFi for AI");
        return;
    }
    show_assistant_status(message);
}

static void set_radio_tile_labels()
{
    lv_label_set_text(ui_Label7, "Play Mix");
    lv_label_set_text(ui_Label9, "Stop");
    lv_label_set_text(ui_Label8, "Hits");
    lv_label_set_text(ui_Label10, "Lounge");
    lv_label_set_text(ui_Label11, "Portugal");
    lv_obj_set_x(ui_Image29, 0);
    lv_obj_set_x(ui_Label7, 0);
    lv_obj_set_x(ui_Image31, 0);
    lv_obj_set_x(ui_Label9, 0);
    lv_img_set_src(ui_Image47, &ui_img_s1_cut2_png);
    lv_img_set_src(ui_Image50, &ui_img_s1_cut1_png);
    lv_img_set_src(ui_Image51, &ui_img_s1_cut2_png);
    lv_img_set_src(ui_Image48, &ui_img_s1_cut2_png);
    lv_img_set_src(ui_Image49, &ui_img_s1_cut2_png);
}

static void set_radio_tile_state(int station_index)
{
    const lv_color_t active = lv_color_hex(0xFFFFFF);
    const lv_color_t idle = lv_color_hex(0x9F9F9F);

    lv_obj_set_style_text_color(ui_Label8, station_index == 0 ? active : idle, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label10, station_index == 1 ? active : idle, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label11, station_index == 2 ? active : idle, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_img_set_src(ui_Image27, station_index == 0 ? &ui_img_radio_hits_on_png : &ui_img_radio_hits_off_png);
    lv_img_set_src(ui_Image34, station_index == 1 ? &ui_img_radio_lounge_on_png : &ui_img_radio_lounge_off_png);
    lv_img_set_src(ui_Image37, station_index == 2 ? &ui_img_radio_portugal_on_png : &ui_img_radio_portugal_off_png);
}
static void set_radio_transport_state(bool playing)
{
    lv_obj_set_style_text_color(ui_Label7, playing ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x9F9F9F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Label9, playing ? lv_color_hex(0x9F9F9F) : lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_img_set_src(ui_Image29, playing ? &ui_img_radio_play_on_png : &ui_img_radio_play_off_png);
    lv_img_set_src(ui_Image31, playing ? &ui_img_radio_stop_off_png : &ui_img_radio_stop_on_png);
}

static void update_radio_ui(bool playing, int station_index)
{
    set_radio_transport_state(playing);
    set_radio_tile_state(station_index);
    update_radio_overlay();
}

static void show_radio_wifi_required()
{
    show_wifi_required("Connect WiFi for radio");
}

static bool ensure_radio_wifi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }
    show_radio_wifi_required();
    return false;
}

static bool radio_uses_remote()
{
    return false;
}

static bool radio_ctrl_start_index(int idx)
{
    if (radio_uses_remote())
    {
        // Donor may need a short warm-up before radio start after BT enable.
        bool remoteOk = false;
        for (int attempt = 0; attempt < 2 && !remoteOk; ++attempt)
        {
            remoteOk = mic_remote_client_radio_start_index(idx);
            if (!remoteOk)
            {
                delay(80);
            }
        }
        if (remoteOk)
        {
            return true;
        }

        // Fallback: keep radio usable even if donor path failed this time.
        radio_vendor_set_output_muted(false);
        return radio_vendor_start_index(idx);
    }
    return radio_vendor_start_index(idx);
}

static bool radio_ctrl_start_random()
{
    if (radio_uses_remote())
    {
        size_t count = 0;
        radio_vendor_stations(&count);
        if (count == 0)
        {
            return false;
        }
        const int idx = random((int)count);
        return radio_ctrl_start_index(idx);
    }
    return radio_vendor_start_random();
}

static bool radio_ctrl_next()
{
    if (radio_uses_remote())
    {
        return mic_remote_client_radio_next();
    }
    return radio_vendor_next();
}

static void radio_ctrl_stop()
{
    if (radio_uses_remote())
    {
        (void)mic_remote_client_radio_stop();
        return;
    }
    radio_vendor_stop();
}

static bool radio_ctrl_is_playing()
{
    if (radio_uses_remote())
    {
        return mic_remote_client_radio_playing();
    }
    return radio_vendor_is_playing();
}

static int radio_ctrl_current_index()
{
    if (radio_uses_remote())
    {
        return mic_remote_client_radio_index();
    }
    return radio_vendor_current_index();
}

static void apply_volume_slider()
{
    uint8_t slider = (uint8_t)lv_slider_get_value(ui_Slider1);
    radio_vendor_set_volume_percent(slider);
}

static void apply_brightness_slider()
{
    uint8_t slider = (uint8_t)lv_slider_get_value(ui_Slider2);
    g_backlight = (uint8_t)map(slider, 0, 100, 20, 255);
        apply_runtime_backlight(g_display_sleeping ? app_settings::kDisplaySleepBrightness : g_backlight);
}

static void ui_event_volume_slider(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
    {
        note_user_activity();
        play_ui_click();
        apply_volume_slider();
    }
}

static void ui_event_backlight_slider(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
    {
        note_user_activity();
        play_ui_click();
        apply_brightness_slider();
    }
}

static void ui_event_ai_chat(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        play_sd_action("self");
    }
}

static void ui_event_ai_news(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        play_sd_action("question");
    }
}

static void ui_event_ai_telegram(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        play_sd_action("murmur");
    }
}

static void ui_event_ai_mic(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        note_user_activity();
        mic_remote_client_force_refresh();
        show_assistant_status(mic_remote_client_status());
    }
}

void ui_main(void)
{
    init_light();
    ui_init();
    do_main_ui_init();
}

// Simple media screen (programmatic, no SLS)
static bool media_radio_playing()
{
    return radio_ctrl_is_playing() && radio_ctrl_current_index() >= 0;
}

static bool media_sd_playing()
{
    return sd_vendor_is_busy();
}

static bool media_play_sd_random()
{
    if (!system_audio_actions_allowed())
    {
        return false;
    }
    return sd_vendor_music_play_current();
}

static bool media_play_sd_shuffle()
{
    if (!system_audio_actions_allowed())
    {
        return false;
    }
    return sd_vendor_music_play_random_current_album();
}

static bool media_play_sd_previous()
{
    if (!system_audio_actions_allowed())
    {
        return false;
    }
    return sd_vendor_music_play_previous();
}

static constexpr uint16_t kMediaCoverRenderSize = 118;

static void media_cover_free_slot()
{
    if (g_media_cover_slot.data)
    {
        heap_caps_free(g_media_cover_slot.data);
        g_media_cover_slot.data = nullptr;
    }
    g_media_cover_slot.capacity = 0;
    g_media_cover_slot.valid = false;
    g_media_cover_slot.checked = false;
    g_media_cover_slot.folder[0] = '\0';
}

static bool media_cover_ensure_capacity(size_t needed)
{
    if (g_media_cover_slot.data && g_media_cover_slot.capacity >= needed)
    {
        return true;
    }
    media_cover_free_slot();
    g_media_cover_slot.data = static_cast<uint8_t *>(heap_caps_malloc(needed, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_media_cover_slot.data)
    {
        g_media_cover_slot.data = static_cast<uint8_t *>(malloc(needed));
    }
    if (!g_media_cover_slot.data)
    {
        return false;
    }
    g_media_cover_slot.capacity = needed;
    return true;
}

static bool media_cover_ends_with_ignore_case(const char *text, const char *suffix)
{
    if (!text || !suffix)
    {
        return false;
    }
    const size_t textLen = strlen(text);
    const size_t suffixLen = strlen(suffix);
    if (suffixLen == 0 || textLen < suffixLen)
    {
        return false;
    }
    const char *start = text + (textLen - suffixLen);
    for (size_t i = 0; i < suffixLen; ++i)
    {
        if (tolower((unsigned char)start[i]) != tolower((unsigned char)suffix[i]))
        {
            return false;
        }
    }
    return true;
}

static bool media_cover_contains_ignore_case(const char *text, const char *needle)
{
    if (!text || !needle || needle[0] == '\0')
    {
        return false;
    }
    const size_t textLen = strlen(text);
    const size_t needleLen = strlen(needle);
    if (textLen < needleLen)
    {
        return false;
    }
    for (size_t i = 0; i <= textLen - needleLen; ++i)
    {
        bool match = true;
        for (size_t j = 0; j < needleLen; ++j)
        {
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)needle[j]))
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            return true;
        }
    }
    return false;
}

static bool media_cover_build_path(char *out_path, size_t out_size, const char *folder, const char *name)
{
    if (!out_path || out_size == 0 || !folder || !name)
    {
        return false;
    }
    const int written = snprintf(out_path, out_size, "%s/%s", folder, name);
    return written > 0 && static_cast<size_t>(written) < out_size;
}

static bool media_cover_extract_folder(const char *track_path, char *out_folder, size_t out_size)
{
    if (!track_path || !out_folder || out_size < 2)
    {
        return false;
    }
    const char *slash = strrchr(track_path, '/');
    if (!slash)
    {
        return false;
    }
    if (slash == track_path)
    {
        strncpy(out_folder, "/", out_size - 1);
        out_folder[out_size - 1] = '\0';
        return true;
    }
    const size_t folderLen = static_cast<size_t>(slash - track_path);
    if (folderLen >= out_size)
    {
        return false;
    }
    memcpy(out_folder, track_path, folderLen);
    out_folder[folderLen] = '\0';
    return true;
}

static bool media_cover_load_bin(const char *path)
{
    if (!path || !sd_vendor_ready())
    {
        return false;
    }
    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        return false;
    }
    const size_t fileSize = static_cast<size_t>(file.size());
    if (fileSize <= sizeof(lv_img_header_t))
    {
        file.close();
        return false;
    }

    lv_img_header_t header{};
    if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header))
    {
        file.close();
        return false;
    }

    const size_t payload = fileSize - sizeof(header);
    if (!media_cover_ensure_capacity(payload))
    {
        file.close();
        return false;
    }
    if (file.read(g_media_cover_slot.data, payload) != static_cast<int>(payload))
    {
        file.close();
        return false;
    }
    file.close();

    g_media_cover_slot.dsc.header = header;
    g_media_cover_slot.dsc.data_size = payload;
    g_media_cover_slot.dsc.data = g_media_cover_slot.data;
    g_media_cover_slot.valid = true;
    return true;
}

static uint16_t media_cover_read_u16_le(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

static uint32_t media_cover_read_u32_le(const uint8_t *bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

static bool media_cover_read_jpeg_size(const uint8_t *data, size_t len, uint16_t *outW, uint16_t *outH)
{
    if (!data || len < 4 || !outW || !outH)
    {
        return false;
    }
    if (data[0] != 0xFF || data[1] != 0xD8)
    {
        return false;
    }

    size_t pos = 2;
    while (pos + 3 < len)
    {
        if (data[pos] != 0xFF)
        {
            ++pos;
            continue;
        }
        while (pos < len && data[pos] == 0xFF)
        {
            ++pos;
        }
        if (pos >= len)
        {
            break;
        }

        const uint8_t marker = data[pos++];
        if (marker == 0xD8 || marker == 0xD9 || marker == 0x01)
        {
            continue;
        }
        if (pos + 1 >= len)
        {
            break;
        }

        const uint16_t segLen = (static_cast<uint16_t>(data[pos]) << 8) | static_cast<uint16_t>(data[pos + 1]);
        if (segLen < 2 || (pos + segLen) > len)
        {
            return false;
        }

        const bool isSof = marker == 0xC0 || marker == 0xC1 || marker == 0xC2 || marker == 0xC3 ||
                           marker == 0xC5 || marker == 0xC6 || marker == 0xC7 ||
                           marker == 0xC9 || marker == 0xCA || marker == 0xCB ||
                           marker == 0xCD || marker == 0xCE || marker == 0xCF;
        if (isSof)
        {
            if (segLen < 7)
            {
                return false;
            }
            const uint16_t h = (static_cast<uint16_t>(data[pos + 3]) << 8) | static_cast<uint16_t>(data[pos + 4]);
            const uint16_t w = (static_cast<uint16_t>(data[pos + 5]) << 8) | static_cast<uint16_t>(data[pos + 6]);
            if (w == 0 || h == 0)
            {
                return false;
            }
            *outW = w;
            *outH = h;
            return true;
        }

        pos += segLen;
    }
    return false;
}

static uint16_t media_cover_scaled_dim(uint16_t src, jpg_scale_t scale)
{
    const uint8_t div = static_cast<uint8_t>(1U << static_cast<uint8_t>(scale));
    uint16_t out = static_cast<uint16_t>(src / div);
    if (out == 0)
    {
        out = 1;
    }
    return out;
}

static jpg_scale_t media_cover_select_jpeg_scale(uint16_t srcW, uint16_t srcH)
{
    jpg_scale_t scale = JPG_SCALE_NONE;
    while (scale < JPG_SCALE_8X)
    {
        const uint16_t scaledW = media_cover_scaled_dim(srcW, scale);
        const uint16_t scaledH = media_cover_scaled_dim(srcH, scale);
        if (scaledW <= 640 && scaledH <= 640)
        {
            break;
        }
        scale = static_cast<jpg_scale_t>(static_cast<uint8_t>(scale) + 1);
    }
    return scale;
}

static bool media_cover_load_jpg(const char *path)
{
    if (!path || !sd_vendor_ready())
    {
        return false;
    }

    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        return false;
    }
    const size_t fileSize = static_cast<size_t>(file.size());
    if (fileSize < 64 || fileSize > (4U * 1024U * 1024U))
    {
        file.close();
        return false;
    }

    uint8_t *jpgData = static_cast<uint8_t *>(heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!jpgData)
    {
        jpgData = static_cast<uint8_t *>(malloc(fileSize));
    }
    if (!jpgData)
    {
        file.close();
        return false;
    }

    if (file.read(jpgData, fileSize) != static_cast<int>(fileSize))
    {
        heap_caps_free(jpgData);
        file.close();
        return false;
    }
    file.close();

    uint16_t srcW = 0;
    uint16_t srcH = 0;
    if (!media_cover_read_jpeg_size(jpgData, fileSize, &srcW, &srcH))
    {
        heap_caps_free(jpgData);
        return false;
    }

    const jpg_scale_t scale = media_cover_select_jpeg_scale(srcW, srcH);
    const uint16_t decW = media_cover_scaled_dim(srcW, scale);
    const uint16_t decH = media_cover_scaled_dim(srcH, scale);
    const size_t decPixels = static_cast<size_t>(decW) * static_cast<size_t>(decH);
    if (decPixels == 0 || decPixels > (900U * 900U))
    {
        heap_caps_free(jpgData);
        return false;
    }

    const size_t decBytes = decPixels * sizeof(uint16_t);
    uint8_t *decoded = static_cast<uint8_t *>(heap_caps_malloc(decBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!decoded)
    {
        decoded = static_cast<uint8_t *>(malloc(decBytes));
    }
    if (!decoded)
    {
        heap_caps_free(jpgData);
        return false;
    }

    const bool decodedOk = jpg2rgb565(jpgData, fileSize, decoded, scale);
    heap_caps_free(jpgData);
    if (!decodedOk)
    {
        heap_caps_free(decoded);
        return false;
    }

    const size_t dstPixels = static_cast<size_t>(kMediaCoverRenderSize) * static_cast<size_t>(kMediaCoverRenderSize);
    const size_t dstBytes = dstPixels * sizeof(uint16_t);
    if (!media_cover_ensure_capacity(dstBytes))
    {
        heap_caps_free(decoded);
        return false;
    }

    const uint16_t *src = reinterpret_cast<const uint16_t *>(decoded);
    uint16_t *dst = reinterpret_cast<uint16_t *>(g_media_cover_slot.data);
    for (uint16_t dy = 0; dy < kMediaCoverRenderSize; ++dy)
    {
        const uint16_t srcY = static_cast<uint16_t>((static_cast<uint32_t>(dy) * decH) / kMediaCoverRenderSize);
        for (uint16_t dx = 0; dx < kMediaCoverRenderSize; ++dx)
        {
            const uint16_t srcX = static_cast<uint16_t>((static_cast<uint32_t>(dx) * decW) / kMediaCoverRenderSize);
            dst[static_cast<size_t>(dy) * kMediaCoverRenderSize + dx] = src[static_cast<size_t>(srcY) * decW + srcX];
        }
    }
    heap_caps_free(decoded);

    lv_img_header_t imgHeader{};
    imgHeader.cf = LV_IMG_CF_TRUE_COLOR;
    imgHeader.w = kMediaCoverRenderSize;
    imgHeader.h = kMediaCoverRenderSize;
    g_media_cover_slot.dsc.header = imgHeader;
    g_media_cover_slot.dsc.data_size = dstBytes;
    g_media_cover_slot.dsc.data = g_media_cover_slot.data;
    g_media_cover_slot.valid = true;
    return true;
}

static bool media_cover_load_bmp(const char *path)
{
    if (!path || !sd_vendor_ready())
    {
        return false;
    }
    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        return false;
    }

    uint8_t header[54];
    if (file.read(header, sizeof(header)) != sizeof(header))
    {
        file.close();
        return false;
    }
    if (header[0] != 'B' || header[1] != 'M')
    {
        file.close();
        return false;
    }

    const uint32_t pixelOffset = media_cover_read_u32_le(&header[10]);
    const uint32_t dibSize = media_cover_read_u32_le(&header[14]);
    const int32_t width = static_cast<int32_t>(media_cover_read_u32_le(&header[18]));
    const int32_t height = static_cast<int32_t>(media_cover_read_u32_le(&header[22]));
    const uint16_t planes = media_cover_read_u16_le(&header[26]);
    const uint16_t bitsPerPixel = media_cover_read_u16_le(&header[28]);
    const uint32_t compression = media_cover_read_u32_le(&header[30]);

    if (dibSize < 40 || planes != 1 || (bitsPerPixel != 24 && bitsPerPixel != 32) || compression != 0)
    {
        file.close();
        return false;
    }

    const int32_t srcW = width > 0 ? width : -width;
    const int32_t srcH = height > 0 ? height : -height;
    if (srcW <= 0 || srcH <= 0 || srcW > 4096 || srcH > 4096)
    {
        file.close();
        return false;
    }

    const bool topDown = height < 0;
    const uint8_t bytesPerPixel = static_cast<uint8_t>(bitsPerPixel / 8);
    const size_t srcStride = static_cast<size_t>(((srcW * bytesPerPixel) + 3) & ~3);
    const uint64_t requiredSize = static_cast<uint64_t>(pixelOffset) + static_cast<uint64_t>(srcStride) * static_cast<uint64_t>(srcH);
    if (requiredSize > static_cast<uint64_t>(file.size()))
    {
        file.close();
        return false;
    }

    const size_t dstPixels = static_cast<size_t>(kMediaCoverRenderSize) * static_cast<size_t>(kMediaCoverRenderSize);
    const size_t dstBytes = dstPixels * sizeof(uint16_t);
    if (!media_cover_ensure_capacity(dstBytes))
    {
        file.close();
        return false;
    }

    uint8_t *row = static_cast<uint8_t *>(malloc(srcStride));
    if (!row)
    {
        file.close();
        return false;
    }

    uint16_t *dst = reinterpret_cast<uint16_t *>(g_media_cover_slot.data);
    for (uint16_t dy = 0; dy < kMediaCoverRenderSize; ++dy)
    {
        const int32_t srcY = (static_cast<int32_t>(dy) * srcH) / static_cast<int32_t>(kMediaCoverRenderSize);
        const int32_t fileY = topDown ? srcY : (srcH - 1 - srcY);
        const size_t rowOffset = static_cast<size_t>(pixelOffset) + static_cast<size_t>(fileY) * srcStride;
        if (!file.seek(rowOffset))
        {
            free(row);
            file.close();
            return false;
        }
        if (file.read(row, srcStride) != static_cast<int>(srcStride))
        {
            free(row);
            file.close();
            return false;
        }

        for (uint16_t dx = 0; dx < kMediaCoverRenderSize; ++dx)
        {
            const int32_t srcX = (static_cast<int32_t>(dx) * srcW) / static_cast<int32_t>(kMediaCoverRenderSize);
            const uint8_t *pixel = row + static_cast<size_t>(srcX) * bytesPerPixel;
            const uint8_t b = pixel[0];
            const uint8_t g = pixel[1];
            const uint8_t r = pixel[2];
            dst[static_cast<size_t>(dy) * kMediaCoverRenderSize + dx] =
                static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }
    }
    free(row);
    file.close();

    lv_img_header_t imgHeader{};
    imgHeader.cf = LV_IMG_CF_TRUE_COLOR;
    imgHeader.w = kMediaCoverRenderSize;
    imgHeader.h = kMediaCoverRenderSize;
    g_media_cover_slot.dsc.header = imgHeader;
    g_media_cover_slot.dsc.data_size = dstBytes;
    g_media_cover_slot.dsc.data = g_media_cover_slot.data;
    g_media_cover_slot.valid = true;
    return true;
}

static bool media_cover_load_path(const char *path)
{
    g_media_cover_slot.valid = false;
    if (!path || !path[0])
    {
        return false;
    }
    if (media_cover_ends_with_ignore_case(path, ".bin"))
    {
        return media_cover_load_bin(path);
    }
    if (media_cover_ends_with_ignore_case(path, ".bmp"))
    {
        return media_cover_load_bmp(path);
    }
    if (media_cover_ends_with_ignore_case(path, ".jpg") || media_cover_ends_with_ignore_case(path, ".jpeg"))
    {
        return media_cover_load_jpg(path);
    }
    return false;
}

static bool media_cover_find_path(const char *folder, char *out_path, size_t out_size)
{
    if (!folder || !out_path || out_size == 0 || !sd_vendor_ready())
    {
        return false;
    }

    const char *preferred[] = {
        "cover.bin", "folder.bin", "album.bin", "front.bin",
        "cover.bmp", "folder.bmp", "album.bmp", "front.bmp",
        "cover.jpg", "folder.jpg", "album.jpg", "front.jpg",
        "cover.jpeg", "folder.jpeg", "album.jpeg", "front.jpeg"};

    for (const char *name : preferred)
    {
        char path[192];
        if (!media_cover_build_path(path, sizeof(path), folder, name))
        {
            continue;
        }
        if (SD.exists(path))
        {
            strncpy(out_path, path, out_size - 1);
            out_path[out_size - 1] = '\0';
            return true;
        }
    }

    File dir = SD.open(folder);
    if (!dir || !dir.isDirectory())
    {
        if (dir)
        {
            dir.close();
        }
        return false;
    }

    char fallbackPath[192] = {0};
    for (;;)
    {
        File f = dir.openNextFile();
        if (!f)
        {
            break;
        }
        if (f.isDirectory())
        {
            f.close();
            continue;
        }

        const char *rawName = f.name();
        if (!rawName || rawName[0] == '\0')
        {
            f.close();
            continue;
        }

        char fullPath[192];
        if (rawName[0] == '/')
        {
            strncpy(fullPath, rawName, sizeof(fullPath) - 1);
            fullPath[sizeof(fullPath) - 1] = '\0';
        }
        else if (!media_cover_build_path(fullPath, sizeof(fullPath), folder, rawName))
        {
            f.close();
            continue;
        }

        const char *nameOnly = strrchr(fullPath, '/');
        nameOnly = nameOnly ? (nameOnly + 1) : fullPath;
        const bool supportedExt = media_cover_ends_with_ignore_case(nameOnly, ".bin") ||
                                  media_cover_ends_with_ignore_case(nameOnly, ".bmp") ||
                                  media_cover_ends_with_ignore_case(nameOnly, ".jpg") ||
                                  media_cover_ends_with_ignore_case(nameOnly, ".jpeg");
        if (!supportedExt)
        {
            f.close();
            continue;
        }

        const bool hintedName = media_cover_contains_ignore_case(nameOnly, "cover") ||
                                media_cover_contains_ignore_case(nameOnly, "folder") ||
                                media_cover_contains_ignore_case(nameOnly, "album") ||
                                media_cover_contains_ignore_case(nameOnly, "front");
        if (hintedName)
        {
            strncpy(out_path, fullPath, out_size - 1);
            out_path[out_size - 1] = '\0';
            f.close();
            dir.close();
            return true;
        }

        if (fallbackPath[0] == '\0')
        {
            strncpy(fallbackPath, fullPath, sizeof(fallbackPath) - 1);
            fallbackPath[sizeof(fallbackPath) - 1] = '\0';
        }
        f.close();
    }
    dir.close();

    if (fallbackPath[0] != '\0')
    {
        strncpy(out_path, fallbackPath, out_size - 1);
        out_path[out_size - 1] = '\0';
        return true;
    }
    return false;
}

static void media_cover_apply_default()
{
    if (!g_media_cover_img)
    {
        return;
    }
    lv_img_set_src(g_media_cover_img, &ui_img_media_note_png);
    lv_img_set_zoom(g_media_cover_img, 132);
    lv_obj_align(g_media_cover_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_img_recolor(g_media_cover_img, lv_color_hex(0xDCE7FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(g_media_cover_img, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void media_cover_apply_loaded()
{
    if (!g_media_cover_img || !g_media_cover_slot.valid)
    {
        return;
    }
    lv_img_set_src(g_media_cover_img, &g_media_cover_slot.dsc);
    lv_img_set_zoom(g_media_cover_img, 256);
    lv_obj_align(g_media_cover_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_img_recolor_opa(g_media_cover_img, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static bool media_cover_update_for_track(const char *track_path)
{
    if (!g_media_cover_img)
    {
        return false;
    }
    if (!track_path || track_path[0] == '\0')
    {
        g_media_cover_track_path[0] = '\0';
        media_cover_apply_default();
        return false;
    }
    if (strcmp(g_media_cover_track_path, track_path) == 0)
    {
        if (g_media_cover_slot.valid)
        {
            media_cover_apply_loaded();
            return true;
        }
        media_cover_apply_default();
        return false;
    }
    strncpy(g_media_cover_track_path, track_path, sizeof(g_media_cover_track_path) - 1);
    g_media_cover_track_path[sizeof(g_media_cover_track_path) - 1] = '\0';

    char folder[144];
    if (!media_cover_extract_folder(track_path, folder, sizeof(folder)))
    {
        media_cover_apply_default();
        return false;
    }

    if (g_media_cover_slot.checked && strcmp(g_media_cover_slot.folder, folder) == 0)
    {
        if (g_media_cover_slot.valid)
        {
            media_cover_apply_loaded();
            return true;
        }
        media_cover_apply_default();
        return false;
    }

    strncpy(g_media_cover_slot.folder, folder, sizeof(g_media_cover_slot.folder) - 1);
    g_media_cover_slot.folder[sizeof(g_media_cover_slot.folder) - 1] = '\0';
    g_media_cover_slot.checked = true;
    g_media_cover_slot.valid = false;

    char coverPath[192] = {0};
    if (media_cover_find_path(folder, coverPath, sizeof(coverPath)) && media_cover_load_path(coverPath))
    {
        media_cover_apply_loaded();
        return true;
    }

    media_cover_apply_default();
    return false;
}


static lv_obj_t *create_media_source_button(lv_obj_t *parent, lv_coord_t x, const void *icon_src, const char *title, MediaSourceKind source)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, 164, 30);
    lv_obj_align(button, LV_ALIGN_CENTER, x, 0);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(button, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(button, media_source_event, LV_EVENT_CLICKED, (void *)(uintptr_t)source);

    lv_obj_t *content = lv_obj_create(button);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 164, 30);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon = lv_img_create(content);
    lv_img_set_src(icon, icon_src);
    lv_img_set_zoom(icon, source == MEDIA_SOURCE_RADIO ? 128 : 116);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = lv_label_create(content);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

    if (source == MEDIA_SOURCE_RADIO)
    {
        lv_obj_align(icon, LV_ALIGN_CENTER, -24, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 20, 0);
    }
    else
    {
        lv_obj_align(icon, LV_ALIGN_CENTER, -20, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 10, 0);
    }

    return button;
}

static void set_media_source_button_state(lv_obj_t *button, bool selected)
{
    if (!button)
    {
        return;
    }

    lv_obj_set_style_bg_color(button, selected ? lv_color_hex(0x1E5E87) : lv_color_hex(0x142232), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, selected ? lv_color_hex(0x2A78A9) : lv_color_hex(0x20344A), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, selected ? lv_color_hex(0xAEEBFF) : lv_color_hex(0x425F78), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(button, lv_color_hex(0xD8F7FF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(button, selected ? 10 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(button, lv_color_hex(0x63DFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(button, selected ? 110 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *content = lv_obj_get_child(button, 0);
    if (!content)
    {
        return;
    }

    lv_obj_t *icon = lv_obj_get_child(content, 0);
    if (icon)
    {
        lv_obj_set_style_opa(icon, selected ? LV_OPA_COVER : LV_OPA_70, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_obj_t *label = lv_obj_get_child(content, 1);
    if (label)
    {
        lv_obj_set_style_text_color(label, selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xA9D7F2), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void update_media_source_buttons()
{
    set_media_source_button_state(g_media_source_radio_btn, g_media_selected_source == MEDIA_SOURCE_RADIO);
    set_media_source_button_state(g_media_source_sd_btn, g_media_selected_source == MEDIA_SOURCE_SD);
}

static void media_source_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    play_ui_click();
    g_media_selected_source = (MediaSourceKind)(uintptr_t)lv_event_get_user_data(e);
    update_media_source_buttons();
    update_media_screen_info();
}

static void media_album_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    note_user_activity();
    play_ui_click();
    const int delta = (int)(intptr_t)lv_event_get_user_data(e);
    if (!sd_vendor_music_select_album_next(delta))
    {
        if (g_media_status_label)
        {
            lv_label_set_text(g_media_status_label, sd_vendor_status());
        }
        return;
    }

    if (media_sd_playing())
    {
        (void)sd_vendor_music_play_current();
    }
    update_media_screen_info();
}

static void media_library_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    note_user_activity();
    play_ui_click();

    const char *action = (const char *)lv_event_get_user_data(e);
    if (!action)
    {
        return;
    }

    // Any library navigation implicitly switches to SD as the active source.
    if (g_media_selected_source != MEDIA_SOURCE_SD)
    {
        g_media_selected_source = MEDIA_SOURCE_SD;
        update_media_source_buttons();
    }

    if (strcmp(action, "Home") == 0)
    {
        stop_all_audio_output();
        (void)sd_vendor_music_refresh();
        (void)sd_vendor_music_browser_go_root();
        media_browser_set_albums();
        const int total = sd_vendor_music_album_count();
        g_media_browser_level = MEDIA_BROWSER_ALBUMS;
        g_media_browser_album_sel = 0;
        g_media_browser_track_sel = 0;
        if (total > 0)
        {
            (void)sd_vendor_music_select_album(0);
        }
        update_media_screen_info();
        return;
    }

    if (strcmp(action, "Scan") == 0)
    {
        media_browser_set_albums();
        (void)start_media_sd_scan();
        return;
    }

    if (strcmp(action, "Up") == 0)
    {
        media_browser_move(-1);
        update_media_screen_info();
        return;
    }

    if (strcmp(action, "Down") == 0)
    {
        media_browser_move(+1);
        update_media_screen_info();
        return;
    }

    if (strcmp(action, "Enter") == 0)
    {
        media_browser_enter();
        return;
    }
}

static lv_obj_t *create_media_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const void *icon_src, const char *title, const char *action)
{
    const bool isToggle = (action && strcmp(action, "Toggle") == 0);
    const bool isTransport = (action && (strcmp(action, "Prev") == 0 || strcmp(action, "Next") == 0));
    const lv_coord_t size = isToggle ? 36 : (isTransport ? 66 : 50);

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, size, size);
    lv_obj_align(button, LV_ALIGN_CENTER, x, y);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, lv_color_hex(isToggle ? 0x12A9FF : 0x1A2035), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, lv_color_hex(isToggle ? 0x0093F5 : 0x29324F), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(button, lv_color_hex(isToggle ? 0xEAF6FF : 0x3C456A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(button, isToggle ? 8 : 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(button, lv_color_hex(isToggle ? 0xA8DEFF : 0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(button, isToggle ? 95 : 70, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(button, media_btn_event, LV_EVENT_CLICKED, (void *)action);

    if (icon_src)
    {
        lv_obj_t *icon = lv_img_create(button);
        lv_img_set_src(icon, icon_src);
        const uint16_t baseZoom = isToggle ? 74 : (isTransport ? 126 : 102);
        const uint16_t scale = isToggle ? 10U : 12U;
        lv_img_set_zoom(icon, (uint16_t)((baseZoom * scale) / 10U));
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, 0);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        if (isToggle)
        {
            g_media_toggle_icon = icon;
        }
        lv_obj_set_style_img_recolor(icon, lv_color_hex(isToggle ? 0xFFFFFF : 0xDCE8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor_opa(icon, isToggle ? LV_OPA_100 : LV_OPA_40, LV_PART_MAIN | LV_STATE_DEFAULT);
        if (action && strcmp(action, "Prev") == 0)
        {
            lv_obj_set_style_img_recolor(icon, lv_color_hex(0xE6EEFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(icon, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    else
    {
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, title ? title : "R");
        lv_obj_set_style_text_font(label, isToggle ? &ui_font_Font4 : &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_color_hex(0xEAF2FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -1);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    }

    return button;
}

static bool queue_media_radio_action(MediaRadioTaskAction action)
{
    if (!system_audio_actions_allowed())
    {
        return false;
    }
    if (g_media_radio_task_pending)
    {
        if (g_media_status_label)
        {
            lv_label_set_text(g_media_status_label, "Radio is busy");
        }
        return false;
    }

    g_media_radio_task_action = action;
    g_media_radio_task_result = false;
    g_media_radio_task_done = false;
    g_media_radio_task_pending = true;

    if (g_media_status_label)
    {
        lv_label_set_text(g_media_status_label, action == MEDIA_RADIO_TASK_NEXT ? "Changing station..." : "Connecting radio...");
    }

    BaseType_t taskOk = xTaskCreatePinnedToCore(
        media_radio_action_task,
        "media_radio_task",
        6144,
        (void *)(uintptr_t)action,
        1,
        &g_media_radio_task,
        1);
    if (taskOk != pdPASS)
    {
        g_media_radio_task_pending = false;
        g_media_radio_task_action = MEDIA_RADIO_TASK_NONE;
        if (g_media_status_label)
        {
            lv_label_set_text(g_media_status_label, "Radio task failed");
        }
        return false;
    }

    return true;
}

static void media_radio_action_task(void *parameter)
{
    const MediaRadioTaskAction action = (MediaRadioTaskAction)(uintptr_t)parameter;
    bool ok = false;

    if (action == MEDIA_RADIO_TASK_PLAY)
    {
        const int stationIndex = radio_ctrl_current_index();
        if (radio_ctrl_is_playing())
        {
            ok = true;
        }
        else if (stationIndex >= 0)
        {
            ok = radio_ctrl_start_index(stationIndex);
        }
        else
        {
            ok = radio_ctrl_start_random();
        }
    }
    else if (action == MEDIA_RADIO_TASK_NEXT)
    {
        ok = radio_ctrl_next();
    }

    g_media_radio_task_result = ok;
    g_media_radio_task_pending = false;
    g_media_radio_task_done = true;
    g_media_radio_task_action = MEDIA_RADIO_TASK_NONE;
    g_media_radio_task = NULL;
    vTaskDelete(NULL);
}

static void service_media_radio_action()
{
    if (!g_media_radio_task_done)
    {
        return;
    }

    g_media_radio_task_done = false;
    update_radio_ui(media_radio_playing(), radio_ctrl_current_index());
    update_media_screen_info();
}

static void set_media_control_enabled(lv_obj_t *obj, bool enabled)
{
    if (!obj)
    {
        return;
    }

    if (enabled)
    {
        lv_obj_clear_state(obj, LV_STATE_DISABLED);
    }
    else
    {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
}

static void set_media_controls_enabled(bool enabled)
{
    set_media_control_enabled(g_media_btn_play, enabled);
    set_media_control_enabled(g_media_btn_pause, enabled);
    set_media_control_enabled(g_media_btn_stop, enabled);
    set_media_control_enabled(g_media_btn_next, enabled);
    set_media_control_enabled(g_media_btn_prev, enabled);
    set_media_control_enabled(g_media_btn_random, enabled);
    set_media_control_enabled(g_media_btn_album_prev, enabled);
    set_media_control_enabled(g_media_btn_album_next, enabled);
    set_media_control_enabled(g_media_btn_album_home, enabled);
    set_media_control_enabled(g_media_btn_album_scan, enabled);
    set_media_control_enabled(g_media_btn_enter, enabled);
    set_media_control_enabled(g_media_source_radio_btn, enabled);
    set_media_control_enabled(g_media_source_sd_btn, enabled);
}

static bool start_media_sd_scan()
{
    if (!system_audio_actions_allowed())
    {
        return false;
    }
    if (g_media_sd_scan_task_pending)
    {
        if (g_media_status_label)
        {
            lv_label_set_text(g_media_status_label, "Scan already running");
        }
        return false;
    }

    if (!sd_vendor_ready())
    {
        if (g_media_status_label)
        {
            lv_label_set_text(g_media_status_label, sd_vendor_status());
        }
        return false;
    }

    g_media_sd_scan_task_result = false;
    g_media_sd_scan_task_done = false;
    g_media_sd_scan_task_pending = true;
    if (g_media_status_label)
    {
        lv_label_set_text(g_media_status_label, "Scanning SD...");
    }
    set_media_controls_enabled(false);

    if (sd_vendor_is_busy() || g_alarm_ringing)
    {
        stop_all_audio_output();
    }

    BaseType_t taskOk = xTaskCreatePinnedToCore(
        media_sd_scan_task,
        "media_sd_scan",
        8192,
        NULL,
        1,
        &g_media_sd_scan_task,
        1);
    if (taskOk != pdPASS)
    {
        g_media_sd_scan_task_pending = false;
        g_media_sd_scan_task = NULL;
        if (g_media_status_label)
        {
            lv_label_set_text(g_media_status_label, "Scan task failed");
        }
        set_media_controls_enabled(true);
        return false;
    }

    return true;
}

static void media_sd_scan_task(void *parameter)
{
    LV_UNUSED(parameter);
    const bool ok = sd_vendor_music_refresh();
    g_media_sd_scan_task_result = ok;
    g_media_sd_scan_task_pending = false;
    g_media_sd_scan_task_done = true;
    g_media_sd_scan_task = NULL;
    vTaskDelete(NULL);
}

static void service_media_sd_scan_task()
{
    if (!g_media_sd_scan_task_done)
    {
        return;
    }

    g_media_sd_scan_task_done = false;
    set_media_controls_enabled(true);
    update_media_screen_info();
    if (g_media_status_label)
    {
        lv_label_set_text(g_media_status_label, g_media_sd_scan_task_result ? "Scan complete" : sd_vendor_status());
    }
    if (g_media_sd_scan_task_result)
    {
        media_browser_set_albums();
    }
}

static void media_browser_reset()
{
    g_media_browser_level = MEDIA_BROWSER_ALBUMS;
    g_media_browser_album_sel = 0;
    g_media_browser_track_sel = 0;
}

static void media_browser_set_albums()
{
    g_media_browser_level = MEDIA_BROWSER_ALBUMS;
    const int total = sd_vendor_music_album_count();
    int seed = sd_vendor_music_album_index();
    if (seed < 0 || seed >= total)
    {
        seed = 0;
    }
    g_media_browser_album_sel = total > 0 ? seed : 0;
    g_media_browser_track_sel = 0;

    if (total > 0)
    {
        (void)sd_vendor_music_select_album(g_media_browser_album_sel);
    }
}

static void media_browser_set_tracks(bool seed_from_current)
{
    g_media_browser_level = MEDIA_BROWSER_TRACKS;
    const int total = sd_vendor_music_count_active();
    int seed = 0;
    if (seed_from_current)
    {
        seed = sd_vendor_music_position_active();
        if (seed < 0)
        {
            seed = 0;
        }
    }
    g_media_browser_track_sel = total > 0 ? max(0, min(total - 1, seed)) : 0;
}

static void media_browser_move(int delta)
{
    if (g_media_sd_scan_task_pending)
    {
        return;
    }

    const int step = (delta < 0) ? -1 : +1;
    if (g_media_browser_level == MEDIA_BROWSER_ALBUMS)
    {
        const int total = sd_vendor_music_album_count();
        if (total <= 0)
        {
            g_media_browser_album_sel = 0;
            return;
        }
        int next = g_media_browser_album_sel + step;
        if (next < 0)
        {
            next = total - 1;
        }
        if (next >= total)
        {
            next = 0;
        }
        g_media_browser_album_sel = next;
        (void)sd_vendor_music_select_album(g_media_browser_album_sel);
        g_media_browser_track_sel = 0;
        return;
    }

    const int total = sd_vendor_music_count_active();
    if (total <= 0)
    {
        g_media_browser_track_sel = 0;
        return;
    }
    int next = g_media_browser_track_sel + step;
    if (next < 0)
    {
        next = total - 1;
    }
    if (next >= total)
    {
        next = 0;
    }
    g_media_browser_track_sel = next;
}

static void media_browser_enter()
{
    if (g_media_sd_scan_task_pending)
    {
        return;
    }

    if (!sd_vendor_ready())
    {
        if (g_media_status_label)
        {
            lv_label_set_text(g_media_status_label, sd_vendor_status());
        }
        return;
    }

    if (g_media_browser_level == MEDIA_BROWSER_ALBUMS)
    {
        if (!sd_vendor_music_select_album(g_media_browser_album_sel))
        {
            if (g_media_status_label)
            {
                lv_label_set_text(g_media_status_label, sd_vendor_status());
            }
            return;
        }

        if (sd_vendor_music_enter_selected_album())
        {
            media_browser_set_albums();
            update_media_screen_info();
            return;
        }

        media_browser_set_tracks(false);
        update_media_screen_info();
        return;
    }

    // Tracks view: play the selected item.
    radio_ctrl_stop();
    const int count = sd_vendor_music_count_active();
    const int sel = count > 0 ? max(0, min(count - 1, g_media_browser_track_sel)) : 0;
    const bool ok = sd_vendor_music_play_active_position(sel);
    if (!ok && g_media_status_label)
    {
        lv_label_set_text(g_media_status_label, sd_vendor_status());
    }
    media_browser_set_tracks(true);
    update_media_screen_info();
}

static String media_album_browser_text(int selected, int total)
{
    if (total <= 0)
    {
        return String("No albums in /songs");
    }

    const int safeSel = max(0, min(total - 1, selected));
    const char *albumName = sd_vendor_music_album_name_at(safeSel);
    String text = "> ";
    text += (albumName && albumName[0]) ? albumName : "-";
    return text;
}

static void update_media_screen_info()
{
    if (!g_media_status_label || !g_media_meta_title_label || !g_media_meta_group_label)
    {
        return;
    }

    if (g_media_sd_scan_task_pending)
    {
        if (g_media_album_label)
        {
            lv_label_set_text(g_media_album_label, "Album: (scanning)");
        }
        if (g_media_track_counter_label)
        {
            lv_label_set_text(g_media_track_counter_label, "--/--");
        }
        if (g_media_next_title_label)
        {
            lv_label_set_text(g_media_next_title_label, "Next: -");
        }
        if (g_media_queue_later_label)
        {
            lv_label_set_text(g_media_queue_later_label, "Later: -");
        }
        if (g_media_progress_fill)
        {
            lv_obj_set_width(g_media_progress_fill, 12);
        }
        if (g_media_progress_arc)
        {
            lv_arc_set_value(g_media_progress_arc, 0);
        }
        return;
    }

    const bool sdPlaying = media_sd_playing();
    const bool radioPlaying = media_radio_playing();
    const bool anyPlaying = sdPlaying || radioPlaying;
    const uint32_t kFillBase = 12U;
    const uint32_t kFillSpan = 224U;
    if (g_media_toggle_icon)
    {
        lv_img_set_src(g_media_toggle_icon, anyPlaying ? &ui_img_media_pause_png : &ui_img_radio_play_on_png);
    }

    if (radioPlaying && !sdPlaying)
    {
        media_cover_apply_default();
        size_t stationCount = 0;
        const VendorRadioStation *stations = radio_vendor_stations(&stationCount);
        int stationIndex = radio_ctrl_current_index();
        if (stationIndex < 0 || stationIndex >= (int)stationCount)
        {
            stationIndex = 0;
        }

        const int nextStation = stationCount > 0 ? (stationIndex + 1) % (int)stationCount : 0;
        const uint32_t fillWidth = stationCount > 0 ? (kFillBase + ((uint32_t)(stationIndex + 1) * kFillSpan) / (uint32_t)stationCount) : kFillBase;

        if (g_media_progress_fill)
        {
            lv_obj_set_width(g_media_progress_fill, (lv_coord_t)fillWidth);
        }
        if (g_media_progress_arc)
        {
            const int arcValue = stationCount > 0 ? (int)(((uint32_t)(stationIndex + 1) * 100U) / (uint32_t)stationCount) : 0;
            lv_arc_set_value(g_media_progress_arc, arcValue);
        }
        if (g_media_track_counter_label)
        {
            if (stationCount > 0)
            {
                lv_label_set_text_fmt(g_media_track_counter_label, "%02d/%02d", stationIndex + 1, (int)stationCount);
            }
            else
            {
                lv_label_set_text(g_media_track_counter_label, "--/--");
            }
        }

        lv_label_set_text(g_media_status_label, "Radio");
        if (radio_uses_remote() && stations && stationCount > 0 && stationIndex >= 0 && stationIndex < (int)stationCount)
        {
            lv_label_set_text(g_media_meta_title_label, stations[stationIndex].name);
            lv_label_set_text(g_media_meta_group_label, "Donor radio");
        }
        else
        {
            lv_label_set_text(g_media_meta_title_label, radio_vendor_current_title());
            lv_label_set_text(g_media_meta_group_label, radio_vendor_current_group());
        }
        if (g_media_next_title_label)
        {
            if (stationCount > 0 && stations)
            {
                lv_label_set_text_fmt(g_media_next_title_label, "Next station: %s", stations[nextStation].name);
            }
            else
            {
                lv_label_set_text(g_media_next_title_label, "Next station: -");
            }
        }
        if (g_media_queue_later_label)
        {
            const char *stationName = (stations && stationCount > 0 && stationIndex >= 0 && stationIndex < (int)stationCount)
                                          ? stations[stationIndex].name
                                          : radio_vendor_current_name();
            lv_label_set_text(g_media_queue_later_label, (stationName && stationName[0]) ? stationName : "Radio");
        }
        if (g_media_queue_more_label_1)
        {
            lv_label_set_text(g_media_queue_more_label_1, "");
        }
        if (g_media_queue_more_label_2)
        {
            lv_label_set_text(g_media_queue_more_label_2, "");
        }
        return;
    }

    const int libraryTrackCount = sd_vendor_music_count();

    if (libraryTrackCount <= 0)
    {
        media_cover_apply_default();
        lv_label_set_text(g_media_status_label, "No library");
        lv_label_set_text(g_media_meta_title_label, "No SD library");
        lv_label_set_text(g_media_meta_group_label, "Need /songs");
        if (g_media_next_title_label)
        {
            lv_label_set_text(g_media_next_title_label, "Next: -");
        }
        if (g_media_queue_later_label)
        {
            lv_label_set_text(g_media_queue_later_label, sd_vendor_status());
        }
        if (g_media_queue_more_label_1)
        {
            lv_label_set_text(g_media_queue_more_label_1, "-");
        }
        if (g_media_queue_more_label_2)
        {
            lv_label_set_text(g_media_queue_more_label_2, "-");
        }
        if (g_media_track_counter_label)
        {
            lv_label_set_text(g_media_track_counter_label, "00/00");
        }
        if (g_media_progress_fill)
        {
            lv_obj_set_width(g_media_progress_fill, (lv_coord_t)kFillBase);
        }
        if (g_media_progress_arc)
        {
            lv_arc_set_value(g_media_progress_arc, 0);
        }
        return;
    }

    const int trackCount = sd_vendor_music_count_active();
    const int trackIndex = sd_vendor_music_position_active();

    if (g_media_browser_level == MEDIA_BROWSER_ALBUMS)
    {
        lv_label_set_text(g_media_status_label, "Albums");

        const int total = sd_vendor_music_album_count();
        const int sel = total > 0 ? max(0, min(total - 1, g_media_browser_album_sel)) : 0;
        const uint32_t fillWidth = total > 0 ? (kFillBase + ((uint32_t)(sel + 1) * kFillSpan) / (uint32_t)total) : kFillBase;

        if (g_media_progress_fill)
        {
            lv_obj_set_width(g_media_progress_fill, (lv_coord_t)fillWidth);
        }
        if (g_media_progress_arc)
        {
            const int arcValue = total > 0 ? (int)(((uint32_t)(sel + 1) * 100U) / (uint32_t)total) : 0;
            lv_arc_set_value(g_media_progress_arc, arcValue);
        }
        if (g_media_track_counter_label)
        {
            if (total > 0)
            {
                lv_label_set_text_fmt(g_media_track_counter_label, "%02d/%02d", sel + 1, total);
            }
            else
            {
                lv_label_set_text(g_media_track_counter_label, "00/00");
            }
        }

        lv_obj_set_width(g_media_meta_title_label, 258);
        lv_label_set_long_mode(g_media_meta_title_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(g_media_meta_title_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(g_media_meta_title_label, media_album_browser_text(sel, total).c_str());
        lv_obj_set_width(g_media_meta_group_label, 258);
        lv_label_set_long_mode(g_media_meta_group_label, LV_LABEL_LONG_DOT);
        lv_label_set_text(g_media_meta_group_label, "Up/Down: select album");

        const int albumTrackCount = sd_vendor_music_count_active();
        if (albumTrackCount > 0)
        {
            const char *previewPath = sd_vendor_music_path_at_active(0);
            if (previewPath && previewPath[0])
            {
                media_cover_update_for_track(previewPath);
            }
            else
            {
                media_cover_apply_default();
            }
        }
        else
        {
            media_cover_apply_default();
        }

        if (g_media_next_title_label)
        {
            lv_obj_set_width(g_media_next_title_label, 258);
            lv_label_set_long_mode(g_media_next_title_label, LV_LABEL_LONG_DOT);
            lv_label_set_text(g_media_next_title_label, "Enter: open  |  Scan: refresh");
        }
        if (g_media_queue_later_label)
        {
            lv_obj_set_width(g_media_queue_later_label, 258);
            lv_label_set_long_mode(g_media_queue_later_label, LV_LABEL_LONG_DOT);
            lv_label_set_text_fmt(g_media_queue_later_label, "Tracks: %d", albumTrackCount);
        }
        return;
    }

    if (trackCount <= 0)
    {
        media_cover_apply_default();
        lv_label_set_text(g_media_status_label, "Album empty");
        lv_label_set_text(g_media_meta_title_label, "No tracks in album");
        lv_label_set_text(g_media_meta_group_label, sd_vendor_music_album_name());
        if (g_media_next_title_label)
        {
            lv_label_set_text(g_media_next_title_label, "Next: -");
        }
        if (g_media_queue_later_label)
        {
            lv_label_set_text(g_media_queue_later_label, "Later: -");
        }
        if (g_media_track_counter_label)
        {
            lv_label_set_text(g_media_track_counter_label, "00/00");
        }
        if (g_media_progress_fill)
        {
            lv_obj_set_width(g_media_progress_fill, (lv_coord_t)kFillBase);
        }
        if (g_media_progress_arc)
        {
            lv_arc_set_value(g_media_progress_arc, 0);
        }
        return;
    }

    const bool usePlayingPos = sdPlaying;
    const int safeIndex = usePlayingPos ? max(0, min(trackCount - 1, (trackIndex >= 0 ? trackIndex : 0)))
                                        : max(0, min(trackCount - 1, g_media_browser_track_sel));
    const uint32_t fillWidth = kFillBase + ((uint32_t)(safeIndex + 1) * kFillSpan) / (uint32_t)trackCount;

    if (g_media_progress_fill)
    {
        lv_obj_set_width(g_media_progress_fill, (lv_coord_t)fillWidth);
    }
    if (g_media_progress_arc)
    {
        const int arcValue = (int)(((uint32_t)(safeIndex + 1) * 100U) / (uint32_t)trackCount);
        lv_arc_set_value(g_media_progress_arc, arcValue);
    }
    if (g_media_track_counter_label)
    {
        lv_label_set_text_fmt(g_media_track_counter_label, "%02d/%02d", safeIndex + 1, trackCount);
    }

    lv_label_set_text(g_media_status_label, sdPlaying ? "Playing" : "Tracks");

    if (!sdPlaying)
    {
        lv_obj_set_width(g_media_meta_title_label, 258);
        lv_label_set_long_mode(g_media_meta_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_align(g_media_meta_title_label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        const char *title = sd_vendor_music_title_at_active(safeIndex);
        const char *group = sd_vendor_music_group_at_active(safeIndex);
        lv_label_set_text(g_media_meta_title_label, (title && title[0]) ? title : "-");
        lv_label_set_text(g_media_meta_group_label, (group && group[0]) ? group : sd_vendor_music_album_name());
        const char *path = sd_vendor_music_path_at_active(safeIndex);
        if (path && path[0])
        {
            media_cover_update_for_track(path);
        }
        else
        {
            media_cover_apply_default();
        }
    }
    else
    {
        lv_label_set_text(g_media_meta_title_label, sd_vendor_music_title());
        lv_label_set_text(g_media_meta_group_label, sd_vendor_music_group());
        const char *path = sd_vendor_music_path();
        if (path && path[0])
        {
            media_cover_update_for_track(path);
        }
        else
        {
            media_cover_apply_default();
        }
    }
    if (g_media_next_title_label)
    {
        const char *nextTitle = sd_vendor_music_title_at_active((safeIndex + 1) % trackCount);
        lv_label_set_text_fmt(g_media_next_title_label, "Next: %s", (nextTitle && nextTitle[0]) ? nextTitle : "-");
    }
    if (g_media_queue_later_label)
    {
        if (trackCount > 1)
        {
            const char *laterTitle = sd_vendor_music_title_at_active((safeIndex + 2) % trackCount);
            lv_label_set_text_fmt(g_media_queue_later_label, "Later: %s", (laterTitle && laterTitle[0]) ? laterTitle : "-");
        }
        else
        {
            lv_label_set_text(g_media_queue_later_label, "Later: -");
        }
    }
    if (g_media_queue_more_label_1)
    {
        lv_label_set_text(g_media_queue_more_label_1, "");
    }
    if (g_media_queue_more_label_2)
    {
        lv_label_set_text(g_media_queue_more_label_2, "");
    }
}
static void ensure_media_screen()
{
    if (g_media_screen)
    {
        return;
    }

    g_media_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(g_media_screen);
    lv_obj_clear_flag(g_media_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(g_media_screen, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_screen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

    g_media_btn_back = lv_btn_create(g_media_screen);
    lv_obj_remove_style_all(g_media_btn_back);
    lv_obj_set_size(g_media_btn_back, 94, 34);
    lv_obj_align(g_media_btn_back, LV_ALIGN_TOP_LEFT, 16, 16);
    lv_obj_clear_flag(g_media_btn_back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(g_media_btn_back, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_back, lv_color_hex(0x1A2033), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_back, lv_color_hex(0x28324D), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(g_media_btn_back, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_media_btn_back, lv_color_hex(0x515C81), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_media_btn_back, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_media_btn_back, media_back_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *backIcon = lv_img_create(g_media_btn_back);
    lv_img_set_src(backIcon, &ui_img_media_back_png);
    lv_img_set_zoom(backIcon, 108);
    lv_obj_align(backIcon, LV_ALIGN_CENTER, -26, 0);
    lv_obj_clear_flag(backIcon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_img_recolor(backIcon, lv_color_hex(0xDCE7FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(backIcon, LV_OPA_50, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *backLabel = lv_label_create(g_media_btn_back);
    lv_label_set_text(backLabel, "Back");
    lv_obj_set_style_text_font(backLabel, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(backLabel, lv_color_hex(0xEAF1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(backLabel, LV_ALIGN_CENTER, 12, 0);
    lv_obj_clear_flag(backLabel, LV_OBJ_FLAG_CLICKABLE);

    g_media_label = lv_label_create(g_media_screen);
    lv_label_set_text(g_media_label, "Player");
    lv_obj_set_style_text_font(g_media_label, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_media_label, lv_color_hex(0xAAB6D8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(g_media_label, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t *topCard = lv_obj_create(g_media_screen);
    lv_obj_remove_style_all(topCard);
    lv_obj_set_size(topCard, 432, 168);
    lv_obj_align(topCard, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_clear_flag(topCard, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(topCard, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(topCard, lv_color_hex(0x1F2238), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(topCard, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(topCard, lv_color_hex(0x303A5D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(topCard, 30, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *artFrame = lv_obj_create(topCard);
    lv_obj_remove_style_all(artFrame);
    lv_obj_set_size(artFrame, 118, 118);
    lv_obj_align(artFrame, LV_ALIGN_LEFT_MID, 14, -4);
    lv_obj_clear_flag(artFrame, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(artFrame, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(artFrame, lv_color_hex(0x111628), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(artFrame, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(artFrame, lv_color_hex(0x2E3658), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(artFrame, 16, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *artIcon = lv_img_create(artFrame);
    lv_img_set_src(artIcon, &ui_img_media_note_png);
    lv_img_set_zoom(artIcon, 132);
    lv_obj_align(artIcon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(artIcon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_img_recolor(artIcon, lv_color_hex(0xDCE7FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(artIcon, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    g_media_cover_img = artIcon;
    media_cover_apply_default();

    lv_obj_t *statusChip = lv_obj_create(topCard);
    lv_obj_remove_style_all(statusChip);
    lv_obj_set_size(statusChip, 84, 24);
    lv_obj_align(statusChip, LV_ALIGN_TOP_RIGHT, -14, 12);
    lv_obj_clear_flag(statusChip, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(statusChip, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(statusChip, lv_color_hex(0x202A44), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(statusChip, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(statusChip, lv_color_hex(0x425179), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(statusChip, 12, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_media_status_label = lv_label_create(statusChip);
    lv_label_set_text(g_media_status_label, "Ready");
    lv_obj_set_width(g_media_status_label, 78);
    lv_obj_set_style_text_font(g_media_status_label, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(g_media_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_media_status_label, lv_color_hex(0xB2E7CB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(g_media_status_label, LV_ALIGN_CENTER, 0, 0);

    g_media_meta_title_label = lv_label_create(topCard);
    lv_label_set_text(g_media_meta_title_label, "Nothing playing");
    lv_obj_set_width(g_media_meta_title_label, 258);
    lv_label_set_long_mode(g_media_meta_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(g_media_meta_title_label, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_media_meta_title_label, lv_color_hex(0xE6ECFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(g_media_meta_title_label, LV_ALIGN_TOP_LEFT, 148, 36);

    g_media_meta_group_label = lv_label_create(topCard);
    lv_label_set_text(g_media_meta_group_label, "");
    lv_obj_set_width(g_media_meta_group_label, 258);
    lv_label_set_long_mode(g_media_meta_group_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(g_media_meta_group_label, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_media_meta_group_label, lv_color_hex(0xB2BCD8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(g_media_meta_group_label, LV_ALIGN_TOP_LEFT, 148, 64);

    g_media_next_title_label = lv_label_create(topCard);
    lv_label_set_text(g_media_next_title_label, "Next: -");
    lv_obj_set_width(g_media_next_title_label, 258);
    lv_label_set_long_mode(g_media_next_title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(g_media_next_title_label, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_media_next_title_label, lv_color_hex(0xC9D4F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(g_media_next_title_label, LV_ALIGN_TOP_LEFT, 148, 88);

    g_media_queue_later_label = lv_label_create(topCard);
    lv_label_set_text(g_media_queue_later_label, "Later: -");
    lv_obj_set_width(g_media_queue_later_label, 258);
    lv_label_set_long_mode(g_media_queue_later_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(g_media_queue_later_label, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_media_queue_later_label, lv_color_hex(0x8D97B8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(g_media_queue_later_label, LV_ALIGN_TOP_LEFT, 148, 108);

    g_media_queue_more_label_1 = nullptr;
    g_media_queue_more_label_2 = nullptr;

    lv_obj_t *progressTrack = lv_obj_create(topCard);
    lv_obj_remove_style_all(progressTrack);
    lv_obj_set_size(progressTrack, 240, 8);
    lv_obj_align(progressTrack, LV_ALIGN_BOTTOM_RIGHT, -14, -12);
    lv_obj_clear_flag(progressTrack, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(progressTrack, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(progressTrack, lv_color_hex(0x3E4666), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(progressTrack, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_media_progress_fill = lv_obj_create(progressTrack);
    lv_obj_remove_style_all(g_media_progress_fill);
    lv_obj_set_size(g_media_progress_fill, 12, 4);
    lv_obj_align(g_media_progress_fill, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_clear_flag(g_media_progress_fill, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(g_media_progress_fill, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_progress_fill, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_media_progress_fill, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *trackChip = lv_obj_create(g_media_screen);
    lv_obj_remove_style_all(trackChip);
    lv_obj_set_size(trackChip, 74, 28);
    lv_obj_align(trackChip, LV_ALIGN_TOP_RIGHT, -22, 24);
    lv_obj_clear_flag(trackChip, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(trackChip, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(trackChip, lv_color_hex(0x1A2035), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(trackChip, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(trackChip, lv_color_hex(0x495577), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(trackChip, 14, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_media_track_counter_label = lv_label_create(trackChip);
    lv_label_set_text(g_media_track_counter_label, "00/00");
    lv_obj_set_style_text_font(g_media_track_counter_label, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_media_track_counter_label, lv_color_hex(0xD7E5FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(g_media_track_counter_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *albumDock = lv_obj_create(g_media_screen);
    lv_obj_remove_style_all(albumDock);
    lv_obj_set_size(albumDock, 432, 42);
    lv_obj_align(albumDock, LV_ALIGN_TOP_MID, 0, 232);
    lv_obj_clear_flag(albumDock, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(albumDock, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(albumDock, lv_color_hex(0x151D30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(albumDock, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(albumDock, lv_color_hex(0x2B3658), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(albumDock, 18, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_media_btn_album_home = lv_btn_create(albumDock);
    lv_obj_remove_style_all(g_media_btn_album_home);
    lv_obj_set_size(g_media_btn_album_home, 60, 30);
    lv_obj_align(g_media_btn_album_home, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_opa(g_media_btn_album_home, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_album_home, lv_color_hex(0x1D2942), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_album_home, lv_color_hex(0x304162), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(g_media_btn_album_home, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_media_btn_album_home, lv_color_hex(0x4A5A83), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_media_btn_album_home, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_media_btn_album_home, media_library_event, LV_EVENT_CLICKED, (void *)"Home");
    lv_obj_t *albumHomeLabel = lv_label_create(g_media_btn_album_home);
    lv_label_set_text(albumHomeLabel, "Home");
    lv_obj_set_style_text_font(albumHomeLabel, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(albumHomeLabel, lv_color_hex(0xE2EDFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(albumHomeLabel, LV_ALIGN_CENTER, 0, 0);

    g_media_btn_album_prev = lv_btn_create(albumDock);
    lv_obj_remove_style_all(g_media_btn_album_prev);
    lv_obj_set_size(g_media_btn_album_prev, 60, 30);
    lv_obj_align(g_media_btn_album_prev, LV_ALIGN_LEFT_MID, 76, 0);
    lv_obj_set_style_bg_opa(g_media_btn_album_prev, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_album_prev, lv_color_hex(0x1D2942), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_album_prev, lv_color_hex(0x304162), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(g_media_btn_album_prev, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_media_btn_album_prev, lv_color_hex(0x4A5A83), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_media_btn_album_prev, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_media_btn_album_prev, media_library_event, LV_EVENT_CLICKED, (void *)"Up");
    lv_obj_t *albumPrevLabel = lv_label_create(g_media_btn_album_prev);
    lv_label_set_text(albumPrevLabel, "Up");
    lv_obj_set_style_text_font(albumPrevLabel, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(albumPrevLabel, lv_color_hex(0xE2EDFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(albumPrevLabel, LV_ALIGN_CENTER, 0, 0);

    g_media_btn_album_next = lv_btn_create(albumDock);
    lv_obj_remove_style_all(g_media_btn_album_next);
    lv_obj_set_size(g_media_btn_album_next, 60, 30);
    lv_obj_align(g_media_btn_album_next, LV_ALIGN_RIGHT_MID, -76, 0);
    lv_obj_set_style_bg_opa(g_media_btn_album_next, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_album_next, lv_color_hex(0x1D2942), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_album_next, lv_color_hex(0x304162), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(g_media_btn_album_next, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_media_btn_album_next, lv_color_hex(0x4A5A83), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_media_btn_album_next, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_media_btn_album_next, media_library_event, LV_EVENT_CLICKED, (void *)"Down");
    lv_obj_t *albumNextLabel = lv_label_create(g_media_btn_album_next);
    lv_label_set_text(albumNextLabel, "Down");
    lv_obj_set_style_text_font(albumNextLabel, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(albumNextLabel, lv_color_hex(0xE2EDFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(albumNextLabel, LV_ALIGN_CENTER, 0, 0);

    g_media_btn_album_scan = lv_btn_create(albumDock);
    lv_obj_remove_style_all(g_media_btn_album_scan);
    lv_obj_set_size(g_media_btn_album_scan, 60, 30);
    lv_obj_align(g_media_btn_album_scan, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_opa(g_media_btn_album_scan, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_album_scan, lv_color_hex(0x1D2942), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_album_scan, lv_color_hex(0x304162), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(g_media_btn_album_scan, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_media_btn_album_scan, lv_color_hex(0x4A5A83), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_media_btn_album_scan, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_media_btn_album_scan, media_library_event, LV_EVENT_CLICKED, (void *)"Scan");
    lv_obj_t *albumScanLabel = lv_label_create(g_media_btn_album_scan);
    lv_label_set_text(albumScanLabel, "Scan");
    lv_obj_set_style_text_font(albumScanLabel, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(albumScanLabel, lv_color_hex(0xE2EDFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(albumScanLabel, LV_ALIGN_CENTER, 0, 0);

    g_media_album_label = nullptr;

    g_media_btn_enter = lv_btn_create(albumDock);
    lv_obj_remove_style_all(g_media_btn_enter);
    lv_obj_set_size(g_media_btn_enter, 60, 30);
    lv_obj_align(g_media_btn_enter, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(g_media_btn_enter, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_enter, lv_color_hex(0x1D2942), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_media_btn_enter, lv_color_hex(0x304162), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(g_media_btn_enter, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_media_btn_enter, lv_color_hex(0x4A5A83), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_media_btn_enter, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_media_btn_enter, media_library_event, LV_EVENT_CLICKED, (void *)"Enter");
    lv_obj_t *enterLabel = lv_label_create(g_media_btn_enter);
    lv_label_set_text(enterLabel, "Enter");
    lv_obj_set_style_text_font(enterLabel, &ui_font_Font3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(enterLabel, lv_color_hex(0xE2EDFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(enterLabel, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *middleDock = lv_obj_create(g_media_screen);
    lv_obj_remove_style_all(middleDock);
    lv_obj_set_size(middleDock, 432, 98);
    lv_obj_align(middleDock, LV_ALIGN_TOP_MID, 0, 278);
    lv_obj_clear_flag(middleDock, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(middleDock, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(middleDock, lv_color_hex(0x1E2239), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(middleDock, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(middleDock, lv_color_hex(0x30395A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(middleDock, 28, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_media_btn_prev = create_media_action_button(middleDock, -146, 0, &ui_img_media_back_png, "Prev", "Prev");
    g_media_btn_next = create_media_action_button(middleDock, 146, 0, &ui_img_media_next_png, "Next", "Next");

    lv_obj_t *centerRing = lv_obj_create(g_media_screen);
    lv_obj_remove_style_all(centerRing);
    lv_obj_set_size(centerRing, 104, 104);
    lv_obj_align(centerRing, LV_ALIGN_TOP_MID, 0, 274);
    lv_obj_clear_flag(centerRing, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(centerRing, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(centerRing, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(centerRing, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(centerRing, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(centerRing, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_media_progress_arc = lv_arc_create(centerRing);
    lv_obj_set_size(g_media_progress_arc, 92, 92);
    lv_obj_align(g_media_progress_arc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(g_media_progress_arc, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_range(g_media_progress_arc, 0, 100);
    lv_arc_set_bg_angles(g_media_progress_arc, 0, 360);
    lv_arc_set_rotation(g_media_progress_arc, 90);
    lv_arc_set_value(g_media_progress_arc, 0);
    lv_obj_remove_style(g_media_progress_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(g_media_progress_arc, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(g_media_progress_arc, lv_color_hex(0x5A617F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(g_media_progress_arc, 7, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(g_media_progress_arc, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_media_progress_arc, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_move_background(g_media_progress_arc);

    g_media_toggle_icon = nullptr;
    g_media_btn_play = create_media_action_button(centerRing, 0, 0, &ui_img_radio_play_on_png, "Play", "Toggle");

    lv_obj_t *bottomDock = lv_obj_create(g_media_screen);
    lv_obj_remove_style_all(bottomDock);
    lv_obj_set_size(bottomDock, 432, 82);
    lv_obj_align(bottomDock, LV_ALIGN_TOP_MID, 0, 392);
    lv_obj_clear_flag(bottomDock, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(bottomDock, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bottomDock, lv_color_hex(0x1E2238), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bottomDock, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(bottomDock, lv_color_hex(0x30385A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bottomDock, 26, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_media_btn_stop = create_media_action_button(bottomDock, -155, 0, &ui_img_radio_stop_on_png, "Stop", "Stop");
    g_media_btn_random = create_media_action_button(bottomDock, -93, 0, NULL, "R", "Random");
    g_media_btn_pause = create_media_action_button(bottomDock, -31, 0, &ui_img_media_pause_png, "Pause", "Pause");
    create_media_action_button(bottomDock, 31, 0, &ui_img_radio_play_on_png, "Play", "Play");
    create_media_action_button(bottomDock, 93, 0, NULL, "-", "VolDown");
    create_media_action_button(bottomDock, 155, 0, NULL, "+", "VolUp");

    lv_obj_t *homePill = lv_obj_create(g_media_screen);
    lv_obj_remove_style_all(homePill);
    lv_obj_set_size(homePill, 96, 8);
    lv_obj_align(homePill, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_clear_flag(homePill, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(homePill, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(homePill, lv_color_hex(0x6B7396), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(homePill, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_media_source_radio_btn = nullptr;
    g_media_source_sd_btn = nullptr;
    update_media_screen_info();
}
static void media_btn_event(lv_event_t *e)
{
    const char *action = (const char *)lv_event_get_user_data(e);
    note_user_activity();
    play_ui_click();

    if (g_system_soft_off && action && strcmp(action, "Stop") != 0 && strcmp(action, "Pause") != 0)
    {
        system_audio_actions_allowed();
        return;
    }

    bool ok = false;
    const bool sdPlaying = media_sd_playing();
    const bool radioPlaying = media_radio_playing();

    if (action && strcmp(action, "Toggle") == 0)
    {
        if (radio_ctrl_is_playing())
        {
            radio_ctrl_stop();
            ok = true;
        }
        else
        {
            ok = media_play_sd_random();
            if (!ok && ensure_radio_wifi())
            {
                ok = queue_media_radio_action(MEDIA_RADIO_TASK_PLAY);
            }
        }
    }
    else if (action && strcmp(action, "Play") == 0)
    {
        ok = sdPlaying ? true : media_play_sd_random();
        if (!ok && ensure_radio_wifi())
        {
            ok = queue_media_radio_action(MEDIA_RADIO_TASK_PLAY);
        }
    }
    else if (action && strcmp(action, "Random") == 0)
    {
        ok = media_play_sd_shuffle();
    }
    else if (action && strcmp(action, "Prev") == 0)
    {
        ok = media_play_sd_previous();
    }
    else if (action && (strcmp(action, "Pause") == 0 || strcmp(action, "Stop") == 0))
    {
        if (radio_ctrl_is_playing())
        {
            radio_ctrl_stop();
        }
        ok = true;
    }
    else if (action && strcmp(action, "Next") == 0)
    {
        if (radioPlaying && !sdPlaying)
        {
            ok = ensure_radio_wifi() ? queue_media_radio_action(MEDIA_RADIO_TASK_NEXT) : false;
        }
        else
        {
            ok = sd_vendor_music_play_next();
        }
    }
    else if (action && (strcmp(action, "VolDown") == 0 || strcmp(action, "VolUp") == 0))
    {
        const int delta = strcmp(action, "VolUp") == 0 ? 6 : -6;
        int volume = (int)radio_vendor_get_volume_percent() + delta;
        volume = max(0, min(100, volume));
        radio_vendor_set_volume_percent((uint8_t)volume);
        lv_slider_set_value(ui_Slider1, volume, LV_ANIM_OFF);
        ok = true;
        if (g_media_status_label)
        {
            lv_label_set_text_fmt(g_media_status_label, "Volume %d%%", volume);
        }
    }

    update_radio_ui(media_radio_playing(), radio_ctrl_current_index());
    if (!ok)
    {
        if (g_media_status_label)
        {
            lv_label_set_text(g_media_status_label, sd_vendor_status());
        }
        return;
    }
    if (g_media_radio_task_pending)
    {
        return;
    }
    update_media_screen_info();
}

static void media_back_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_RELEASED && code != LV_EVENT_CLICKED)
    {
        return;
    }
    note_user_activity();
    play_ui_click();
    _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen1_screen_init);
}

static void open_media_screen()
{
    ensure_media_screen();
    if (!sd_vendor_music_loaded())
    {
        (void)start_media_sd_scan();
    }
    media_browser_reset();
    g_media_browser_level = MEDIA_BROWSER_TRACKS;
    update_media_screen_info();
    _ui_screen_change(&g_media_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, NULL);
}

static void media_open_event(lv_event_t *e)
{
    LV_UNUSED(e);
    play_ui_click();
    open_media_screen();
}

static void stop_all_audio_output()
{
    if (g_tap_listen_active)
    {
        stop_tap_listen();
    }

    alarm_stop_ringing();
    radio_ctrl_stop();
    update_radio_ui(false, radio_ctrl_current_index());
    update_media_screen_info();
}

static bool system_audio_actions_allowed()
{
    if (!g_system_soft_off)
    {
        return true;
    }

    if (g_media_status_label)
    {
        lv_label_set_text(g_media_status_label, "System off");
    }
    show_assistant_status("System off");
    update_home_labels();
    return false;
}

static void set_system_soft_off(bool enabled)
{
    if (g_system_soft_off == enabled)
    {
        if (enabled)
        {
    apply_runtime_backlight(app_settings::kSystemSoftOffBrightness);
        }
        else
        {
            apply_runtime_backlight(g_backlight);
        }
        update_home_launch_buttons();
        update_home_labels();
        return;
    }

    g_system_soft_off = enabled;
    if (enabled)
    {
        g_system_resume_mic_enabled = g_ai_voice_enabled;
        g_ai_voice_enabled = false;
        stop_all_audio_output();
        g_display_sleeping = false;
        g_display_wake_hold_until_ms = 0;
    apply_runtime_backlight(app_settings::kSystemSoftOffBrightness);
        set_tap_listen_result("System off", false, 1800);
    }
    else
    {
        g_ai_voice_enabled = false;
        g_last_user_activity_ms = millis();
        g_last_sound_activity_ms = g_last_user_activity_ms;
        g_display_sleeping = false;
        g_display_wake_hold_until_ms = g_last_user_activity_ms + app_settings::kDisplaySleepWakeHoldMs;
        apply_runtime_backlight(g_backlight);
        set_tap_listen_result("System on", false, 1800);
    }

    update_settings_labels();
    update_home_labels();
    update_home_launch_buttons();
    update_ai_labels();
    update_media_screen_info();
}

static void home_launch_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
    {
        return;
    }

    note_user_activity();
    const HomeLaunchTarget target = (HomeLaunchTarget)(uintptr_t)lv_event_get_user_data(e);
    if (target == HOME_LAUNCH_ON)
    {
        play_ui_click();
        set_system_soft_off(false);
        return;
    }
    if (target == HOME_LAUNCH_OFF)
    {
        play_ui_click();
        set_system_soft_off(true);
        return;
    }
    if (g_system_soft_off)
    {
        update_home_labels();
        return;
    }
    if (target == HOME_LAUNCH_RADIO)
    {
        change_voice_screen({&ui_Screen11, &ui_Screen11_screen_init});
    }
    else if (target == HOME_LAUNCH_PLAYER)
    {
        open_media_screen();
    }
}

static void create_home_launch_buttons()
{
    if (g_home_radio_btn || g_home_player_btn || g_home_voice_btn || g_home_off_btn || !ui_Screen1)
    {
        return;
    }
    const struct HomeButtonSpec {
        lv_obj_t **slot;
        lv_coord_t x;
        const void *icon;
        uint16_t zoom;
        lv_coord_t icon_x;
        const char *label;
        lv_coord_t label_x;
        lv_coord_t label_w;
        HomeLaunchTarget target;
    } specs[] = {
        {&g_home_radio_btn, -180, &ui_img_radio_play_on_png, 128, -14, "Radio", -6, 64, HOME_LAUNCH_RADIO},
        {&g_home_player_btn, -60, &ui_img_media_note_png, 121, -14, "Player", -6, 64, HOME_LAUNCH_PLAYER},
        {&g_home_voice_btn, 60, &ui_img_s3_switch1_on_png, 133, -8, "On", -4, 58, HOME_LAUNCH_ON},
        {&g_home_off_btn, 180, &ui_img_radio_stop_on_png, 143, -4, "Off", -4, 58, HOME_LAUNCH_OFF},
    };
    for (const auto &spec : specs)
    {
        lv_obj_t *button = lv_btn_create(ui_Screen1);
        lv_obj_remove_style_all(button);
        lv_obj_set_size(button, 104, 42);
        lv_obj_align(button, LV_ALIGN_BOTTOM_MID, spec.x, -58);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(button, LV_OPA_80, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x162231), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x20344A), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(button, lv_color_hex(0x5E86A8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(button, lv_color_hex(0xAEEBFF), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(button, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(button, 12, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_shadow_color(button, lv_color_hex(0x63DFFF), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_add_event_cb(button, home_launch_event, LV_EVENT_CLICKED, (void *)(uintptr_t)spec.target);
        lv_obj_t *icon = lv_img_create(button);
        lv_img_set_src(icon, spec.icon);
        lv_img_set_zoom(icon, spec.zoom);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, spec.icon_x, 0);
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_label_set_text(label, spec.label);
        lv_obj_set_style_text_font(label, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_width(label, spec.label_w);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(label, LV_ALIGN_RIGHT_MID, spec.label_x, 0);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(button);
        *spec.slot = button;
    }
    update_home_launch_buttons();
}
static void create_settings_media_button()
{
    if (g_settings_media_btn)
    {
        return;
    }

    g_settings_media_btn = lv_obj_create(ui_Screen10);
    lv_obj_remove_style_all(g_settings_media_btn);
    lv_obj_set_size(g_settings_media_btn, 374, 52);
    lv_obj_align(g_settings_media_btn, LV_ALIGN_CENTER, 0, 170);
    lv_obj_add_flag(g_settings_media_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_settings_media_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(g_settings_media_btn, LV_OPA_80, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_settings_media_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(g_settings_media_btn, lv_color_hex(0x162231), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_settings_media_btn, lv_color_hex(0x20344A), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(g_settings_media_btn, lv_color_hex(0x5E86A8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_settings_media_btn, lv_color_hex(0xAEEBFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(g_settings_media_btn, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_settings_media_btn, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(g_settings_media_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(g_settings_media_btn, 12, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(g_settings_media_btn, lv_color_hex(0x63DFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(g_settings_media_btn, media_open_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *rowIcon = lv_img_create(g_settings_media_btn);
    lv_img_set_src(rowIcon, &ui_img_media_note_png);
    lv_img_set_zoom(rowIcon, 160);
    lv_obj_align(rowIcon, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_clear_flag(rowIcon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *rowLabel = lv_label_create(g_settings_media_btn);
    lv_label_set_text(rowLabel, "Media player");
    lv_obj_set_style_text_font(rowLabel, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(rowLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(rowLabel, LV_ALIGN_LEFT_MID, 54, 0);

    lv_obj_t *openChip = lv_btn_create(g_settings_media_btn);
    lv_obj_remove_style_all(openChip);
    lv_obj_set_size(openChip, 132, 40);
    lv_obj_align(openChip, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_clear_flag(openChip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(openChip, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(openChip, lv_color_hex(0x1E5E87), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(openChip, lv_color_hex(0x2A78A9), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(openChip, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(openChip, lv_color_hex(0xAEEBFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(openChip, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(openChip, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(openChip, 12, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(openChip, lv_color_hex(0x63DFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(openChip, media_open_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *openIcon = lv_img_create(openChip);
    lv_img_set_src(openIcon, &ui_img_radio_play_on_png);
    lv_img_set_zoom(openIcon, 132);
    lv_obj_align(openIcon, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_clear_flag(openIcon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *openLabel = lv_label_create(openChip);
    lv_label_set_text(openLabel, "Open");
    lv_obj_set_style_text_font(openLabel, &ui_font_Font4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(openLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(openLabel, LV_ALIGN_RIGHT_MID, -10, 0);
}
void do_main_ui_init(void)
{
    lv_keyboard_set_textarea(ui_Keyboard4, ui_TextArea1);
    lv_obj_add_event_cb(ui_TextArea1, wifi_ui_textarea_focus_cb, LV_EVENT_FOCUSED, ui_Keyboard4);
    setup_wifi_city_input();
    lv_obj_add_event_cb(ui_Button2, ui_event_Button_scan, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Keyboard4, ui_event_Key_Ok, LV_EVENT_READY, NULL);

    lv_obj_add_event_cb(ui_Image26, ui_event_All_on, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image32, ui_event_All_off, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image24, ui_event_All_on, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image25, ui_event_All_off, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image29, ui_event_All_on, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Label7, ui_event_All_on, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image31, ui_event_All_off, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Label9, ui_event_All_off, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(ui_Image24, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Image25, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Image29, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label7, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Image31, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Label9, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(ui_Image30, ui_event_light1, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image35, ui_event_light2, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image38, ui_event_light3, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image28, ui_event_light1, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image33, ui_event_light2, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_Image36, ui_event_light3, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(ui_Image28, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Image33, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui_Image36, LV_OBJ_FLAG_CLICKABLE);
    _ui_flag_modify(ui_Image26, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Image32, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Image30, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Image35, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Image38, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    lv_img_set_zoom(ui_Image29, 235);
    lv_img_set_zoom(ui_Image31, 235);
    lv_img_set_zoom(ui_Image27, 235);
    lv_img_set_zoom(ui_Image34, 235);
    lv_img_set_zoom(ui_Image37, 235);
    lv_obj_add_event_cb(ui_Slider1, ui_event_volume_slider, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Slider2, ui_event_backlight_slider, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_Button3, assistant_card_event_cb, LV_EVENT_ALL, (void *)ASSISTANT_ACTION_SELF);
    lv_obj_add_event_cb(ui_Button4, assistant_card_event_cb, LV_EVENT_ALL, (void *)ASSISTANT_ACTION_ASK);
    lv_obj_add_event_cb(ui_Button5, assistant_card_event_cb, LV_EVENT_ALL, (void *)ASSISTANT_ACTION_MURMUR);
    _ui_flag_modify(ui_Button6, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Image67, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    lv_obj_clear_flag(ui_Button6, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Image67, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_event_cb(ui_Image16, ui_event_Image16);
    lv_obj_remove_event_cb(ui_Image17, ui_event_Image17);
    _ui_flag_modify(ui_Image16, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Image17, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    lv_obj_clear_flag(ui_Image16, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ui_Image17, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(ui_Image16, -1000, -1000);
    lv_obj_set_pos(ui_Image17, -1000, -1000);

    set_radio_tile_labels();
    init_overlay_ui();
    configure_home_weather_labels();
    // Show weather area immediately on boot even before first API response.
    lv_label_set_text(ui_Label2, "Weather loading");
    lv_label_set_text(ui_Label4, "--");
    lv_label_set_text(ui_Label5, "--");
    lv_label_set_text(ui_Label6, "--");
    lv_label_set_text(ui_Label15, "C");
    lv_label_set_text(ui_Label16, "m/s");
    lv_label_set_text(ui_Label17, "%");
    _ui_flag_modify(ui_Image6, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Image7, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Image8, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Label15, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Label16, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(ui_Label17, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    update_weather_icon(true);
    update_home_background();
    enable_home_gesture_bubble();
    lv_slider_set_value(ui_Slider1, radio_vendor_get_volume_percent(), LV_ANIM_OFF);
    lv_slider_set_value(ui_Slider2, map(g_backlight, 20, 255, 0, 100), LV_ANIM_OFF);
    apply_volume_slider();
    apply_brightness_slider();
    update_radio_ui(media_radio_playing(), radio_ctrl_current_index());

    ensure_media_screen();
    create_home_launch_buttons();
    if (g_boot_wait_wifi_on_start && g_wifi_connect_state == 1)
    {
        show_boot_black_screen();
    }
}

void init_light()
{
    Serial.println("[wxfix] init_light enter");
    wake_logic_reset();
    if (kDonorWakeGpioEnabled)
    {
        pinMode(kDonorWakeGpioInPin, INPUT_PULLDOWN);
    }
    donor_eq_uart_init();
    audio_visualizer_init();
    mic_remote_client_init();
    net_worker_begin(1, 1);
    radio_vendor_init();
    sd_vendor_init();
    weather_visuals_init();
    home_bg_sd_assets_init();
    load_saved_wifi_credentials();
    g_boot_black_active = false;
    g_boot_wait_wifi_on_start = (ssid.length() > 0 && WiFi.status() != WL_CONNECTED);
    if (g_boot_wait_wifi_on_start && g_wifi_connect_state == 0)
    {
        if (!start_wifi_connect_task())
        {
            g_boot_wait_wifi_on_start = false;
        }
    }
    // Start NTP/weather only after Wi-Fi connects to preserve SRAM during WPA handshake.
    g_time_task_started = false;
    Serial.println("[wxfix] defer weather task until WiFi connected");
    randomSeed((uint32_t)micros());
    schedule_next_ambient(true);
}

static void donor_eq_uart_init()
{
    gDonorEqSerial.setTxBufferSize(1024);
    gDonorEqSerial.begin(kDonorEqBaud, SERIAL_8N1, -1, kDonorEqUartTxPin);
}

static void donor_eq_uart_push(const uint8_t *bars, size_t count, bool active)
{
    if (!bars || count == 0)
    {
        return;
    }

    const size_t n = min<size_t>(count, 8);
    uint8_t frame[2 + 1 + 1 + 8 + 1] = {0};
    frame[0] = 0xA5;
    frame[1] = 0x5A;
    frame[2] = gDonorEqSeq++;
    frame[3] = active ? 1 : 0;
    for (size_t i = 0; i < 8; ++i)
    {
        frame[4 + i] = (i < n) ? bars[i] : 0;
    }

    uint8_t crc = 0;
    for (size_t i = 0; i < (sizeof(frame) - 1); ++i)
    {
        crc ^= frame[i];
    }
    frame[sizeof(frame) - 1] = crc;
    gDonorEqSerial.write(frame, sizeof(frame));
}

void light1_on()
{
    lv_img_set_src(ui_Image30, &ui_img_s3_switch1_on_png);
    lv_img_set_src(ui_Image27, &ui_img_s3_light1_on_png);
    light1_status = true;
}

void light1_off()
{
    digitalWrite(40, LOW);
    lv_img_set_src(ui_Image30, &ui_img_s3_switch1_off_png);
    lv_img_set_src(ui_Image27, &ui_img_s3_light1_off_png);
    light1_status = false;
}

boolean get_light1_status()
{
    return light1_status;
}

void light2_on()
{
    digitalWrite(2, HIGH);
    lv_img_set_src(ui_Image35, &ui_img_s3_switch1_on_png);
    lv_img_set_src(ui_Image34, &ui_img_s3_light1_on_png);
    light2_status = true;
}

void light2_off()
{
    digitalWrite(2, LOW);
    lv_img_set_src(ui_Image35, &ui_img_s3_switch1_off_png);
    lv_img_set_src(ui_Image34, &ui_img_s3_light1_off_png);
    light2_status = false;
}

boolean get_light2_status()
{
    return light2_status;
}

void light3_on()
{
    digitalWrite(1, HIGH);
    lv_img_set_src(ui_Image38, &ui_img_s3_switch1_on_png);
    lv_img_set_src(ui_Image37, &ui_img_s3_light1_on_png);
    light3_status = true;
}

void light3_off()
{
    digitalWrite(1, LOW);
    lv_img_set_src(ui_Image38, &ui_img_s3_switch1_off_png);
    lv_img_set_src(ui_Image37, &ui_img_s3_light1_off_png);
    light3_status = false;
}

boolean get_light3_status()
{
    return light3_status;
}

void ui_event_All_on(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    LV_UNUSED(lv_event_get_target(e));
    if (event_code == LV_EVENT_CLICKED)
    {
        note_user_activity();
        play_ui_click();
        if (!system_audio_actions_allowed())
        {
            return;
        }
        if (!ensure_radio_wifi())
        {
            return;
        }
        bool ok = queue_media_radio_action(MEDIA_RADIO_TASK_PLAY);
        if (!ok && !g_media_radio_task_pending)
        {
            // Last-resort direct start if task queue was not accepted.
            ok = radio_ctrl_start_random();
            update_radio_ui(ok, radio_ctrl_current_index());
        }
    }
}

void ui_event_All_off(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    LV_UNUSED(lv_event_get_target(e));
    if (event_code == LV_EVENT_CLICKED)
    {
        note_user_activity();
        play_ui_click();
        radio_ctrl_stop();
        update_radio_ui(false, radio_ctrl_current_index());
    }
}

void ui_event_light1(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    LV_UNUSED(lv_event_get_target(e));
    if (event_code == LV_EVENT_CLICKED)
    {
        note_user_activity();
        if (!system_audio_actions_allowed())
        {
            return;
        }
        if (!ensure_radio_wifi())
        {
            return;
        }
        bool ok = radio_ctrl_start_index(0);
        update_radio_ui(ok, 0);
    }
}

void ui_event_light2(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    LV_UNUSED(lv_event_get_target(e));
    if (event_code == LV_EVENT_CLICKED)
    {
        note_user_activity();
        if (!system_audio_actions_allowed())
        {
            return;
        }
        if (!ensure_radio_wifi())
        {
            return;
        }
        bool ok = radio_ctrl_start_index(1);
        update_radio_ui(ok, 1);
    }
}

void ui_event_light3(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    LV_UNUSED(lv_event_get_target(e));
    if (event_code == LV_EVENT_CLICKED)
    {
        note_user_activity();
        if (!system_audio_actions_allowed())
        {
            return;
        }
        if (!ensure_radio_wifi())
        {
            return;
        }
        bool ok = radio_ctrl_start_index(2);
        update_radio_ui(ok, 2);
    }
}

void ui_event_Image16(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    LV_UNUSED(lv_event_get_target(e));
    if (event_code == LV_EVENT_CLICKED)
    {
        note_user_activity();
        play_ui_click();
        g_ai_voice_enabled = false;
        update_settings_labels();
        show_assistant_status("Mic disabled.");
    }
}
void ui_event_Image17(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    LV_UNUSED(lv_event_get_target(e));
    if (event_code == LV_EVENT_CLICKED)
    {
        note_user_activity();
        play_ui_click();
        g_ai_voice_enabled = false;
        update_settings_labels();
        show_assistant_status("Mic disabled.");
    }
}

void ui_event_Button_scan(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    if (event_code == LV_EVENT_CLICKED)
    {
        note_user_activity();
        play_ui_click();
        lv_label_set_text(ui_Label12, wifi_ui_status_scanning());
        WiFi.disconnect(false, false);
        delay(150);
        WiFi.scanDelete();
        wifi_nums = scanNetworks();
        Serial.println(wifi_nums);
        String wifi_name = wifi_ui_networks_to_dropdown_options(wifi_nums);
        for (int i = 0; i < wifi_nums; i++)
        {
            Serial.print(i);
            Serial.print(":");
            Serial.print(WiFi.SSID(i));
            Serial.print(" ");
            Serial.print(WiFi.RSSI(i));
            Serial.print(" ");
            Serial.print(transEncryptionType(WiFi.encryptionType(i)));
            Serial.println("");
        }
        lv_dropdown_set_options(ui_Dropdown2, wifi_name.c_str());
        lv_dropdown_set_selected(ui_Dropdown2, 0);
        lv_label_set_text(ui_Label12, wifi_ui_status_after_scan(wifi_nums));
    }
}

void scan_wifi_task(void *pvParameters)
{
    vTaskDelete(NULL);
}

void connect_wifi_task(void *pvParameters)
{
    wifi_manager_connect_blocking(ssid, pswd, &g_wifi_connect_state, ensure_panel_link_ap);
    vTaskDelete(NULL);
}

const char *ntpServer = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const char *timeZone = "EET-2EEST,M3.5.0/3,M10.5.0/4";

void getNtpTime(void *pvParameters)
{
    wifi_manager_ntp_begin(ntpServer, ntpServer2, timeZone);
    NTPState = wifi_manager_ntp_wait_initial_sync(30, 500);
    Serial.println(NTPState ? "time sync ok" : "time sync pending");

    for (;;)
    {
        WifiManagerTimeSnapshot snap;
        wifi_manager_ntp_read_time(&snap);

        const bool ntpNow = snap.ntp_synced;
        if (ntpNow != NTPState)
        {
            NTPState = ntpNow;
            Serial.println(NTPState ? "time sync ok" : "time sync pending");
        }

        currentTime_year = snap.year;
        currentTime_mouth = snap.month;
        currentTime_day = snap.day;
        currentTime_hour = snap.hour;
        currentTime_minute = snap.minute;
        currentTime_second = snap.second;
        currentTime_week = snap.weekday;
        timeNow = snap.unix_time;
        if (currentTime_hour <= 9)
        {
            hourText = "0" + String(currentTime_hour);
        }
        else
        {
            hourText = String(currentTime_hour);
        }
        if (currentTime_minute <= 9)
        {
            minuteText = "0" + String(currentTime_minute);
        }
        else
        {
            minuteText = String(currentTime_minute);
        }

        yearText = String(currentTime_year);
        mouthText = String(currentTime_mouth);
        dayText = String(currentTime_day);
        if (currentTime_mouth <= 9)
        {
            mouthText = "0" + String(currentTime_mouth);
        }
        else
        {
            mouthText = String(currentTime_mouth);
        }
        if (currentTime_day <= 9)
        {
            dayText = "0" + String(currentTime_day);
        }
        else
        {
            dayText = String(currentTime_day);
        }
        topDateText = yearText + "/" + mouthText + "/" + dayText;
        g_clock_timezone_text = snap.is_dst ? "Kyiv time - EEST" : "Kyiv time - EET";

        topTimeText = hourText + ":" + minuteText;

        yearText = String(currentTime_year);
        mouthText = String(currentTime_mouth);
        dayText = String(currentTime_day);
        if (currentTime_mouth <= 9)
        {
            mouthText = "0" + String(currentTime_mouth);
        }
        else
        {
            mouthText = String(currentTime_mouth);
        }
        if (currentTime_day <= 9)
        {
            dayText = "0" + String(currentTime_day);
        }
        else
        {
            dayText = String(currentTime_day);
        }
        topDateText = yearText + "/" + mouthText + "/" + dayText;
        if (currentTime_week == 6)
        {
            weekText = "Sat";
        }
        else if (currentTime_week == 0)
        {
            weekText = "Sun";
        }
        else if (currentTime_week == 3)
        {
            weekText = "Wed";
        }
        else if (currentTime_week == 2)
        {
            weekText = "Tue";
        }
        else if (currentTime_week == 1)
        {
            weekText = "Mon";
        }
        else if (currentTime_week == 4)
        {
            weekText = "Thu";
        }
        else if (currentTime_week == 5)
        {
            weekText = "Fri";
        }

        topDateText = yearText + "/" + mouthText + "/" + dayText;

        g_clock_dirty = true;

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void ui_event_Key_Ok(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_READY)
    {
        note_user_activity();
        play_ui_action();

        lv_obj_t *active_input = ui_Keyboard4 ? lv_keyboard_get_textarea(ui_Keyboard4) : NULL;
        if (active_input == g_city_textarea)
        {
            g_city_query = lv_textarea_get_text(g_city_textarea);
            g_city_query.trim();
            lv_obj_clear_state(g_city_textarea, LV_STATE_FOCUSED);
            lv_keyboard_set_textarea(ui_Keyboard4, ui_TextArea1);
            if (g_prefs.begin("wifi", false))
            {
                g_prefs.putFloat("wlat", g_weather_saved_lat);
                g_prefs.putFloat("wlon", g_weather_saved_lon);
                g_prefs.end();
            }
            lv_label_set_text(ui_Label12, g_city_query.length() > 0 ? "City saved" : "City cleared");
            return;
        }

        lv_obj_add_flag(ui_Button2, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_Label12, wifi_ui_status_connecting());
        lv_obj_clear_state(ui_TextArea1, LV_STATE_FOCUSED);

        char buf[32];
        lv_dropdown_get_selected_str(ui_Dropdown2, buf, sizeof(buf));
        ssid = String(buf);
        pswd = lv_textarea_get_text(ui_TextArea1);
        if (g_city_textarea)
        {
            g_city_query = lv_textarea_get_text(g_city_textarea);
            g_city_query.trim();
        }

        if (g_wifi_connect_state == 1)
        {
            return;
        }
        if (!start_wifi_connect_task())
        {
            lv_label_set_text(ui_Label12, wifi_ui_status_connect_task_failed());
        }
    }
}

String getIps(void)
{
    String ip = "";
    const char *url = "http://ip-api.com/json/?fields=query";
    String payload;
    NetWorkerHttpOptions net_opts;
    net_opts.insecure_tls = false;
    net_opts.connect_timeout_ms = 4000;
    net_opts.timeout_ms = 5000;
    int httpCode = net_worker_http_get(url, &payload, &net_opts);
    if (httpCode == HTTP_CODE_OK)
    {
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, payload) == DeserializationError::Ok)
        {
            ip = doc["query"].as<String>();
        }
    }
    else
    {
        Serial.printf("[weather] ip lookup failed: %d\n", httpCode);
    }
    return ip;
}

float *getCityPosition(String ips)
{
    float *position = new float[2];
    position[0] = 0.0f;
    position[1] = 0.0f;

    String url = "http://ip-api.com/json";
    if (ips.length() > 0)
    {
        url += "/" + ips;
    }
    url += "?fields=status,lat,lon,city,country,countryCode";

    String payload;
    NetWorkerHttpOptions net_opts;
    net_opts.insecure_tls = false;
    net_opts.connect_timeout_ms = 4000;
    net_opts.timeout_ms = 5000;
    int httpCode = net_worker_http_get(url, &payload, &net_opts);
    if (httpCode == HTTP_CODE_OK)
    {
        DynamicJsonDocument doc(512);
        if (deserializeJson(doc, payload) == DeserializationError::Ok &&
            doc["status"].as<String>() == "success")
        {
            position[0] = doc["lat"].as<float>();
            position[1] = doc["lon"].as<float>();
            g_city_name = doc["city"].as<String>();
            g_country_name = doc["country"].as<String>();
            g_country_code = doc["countryCode"].as<String>();
            g_country_code.trim();
            g_country_code.toUpperCase();
        }
    }
    else
    {
        Serial.printf("[weather] geo lookup failed: %d\n", httpCode);
    }
    return position;
}

static const char *weather_code_to_text(int code, bool isDay)
{
    switch (code)
    {
    case 0: return isDay ? "Clear" : "Clear night";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Cloudy";
    case 45:
    case 48: return "Fog";
    case 51:
    case 53:
    case 55: return "Drizzle";
    case 56:
    case 57: return "Freezing drizzle";
    case 61:
    case 63:
    case 65: return "Rain";
    case 66:
    case 67: return "Freezing rain";
    case 71:
    case 73:
    case 75:
    case 77: return "Snow";
    case 80:
    case 81:
    case 82: return "Rain showers";
    case 85:
    case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96:
    case 99: return "Storm hail";
    default: return "Weather";
    }
}

static int map_wttr_code_to_visual_code(int wttrCode)
{
    switch (wttrCode)
    {
    case 113: return 0;   // clear
    case 116: return 2;   // partly cloudy
    case 119:
    case 122: return 3;   // cloudy/overcast
    case 143:
    case 248:
    case 260: return 45;  // mist/fog
    case 176:
    case 263:
    case 266:
    case 293:
    case 296:
    case 299:
    case 302:
    case 305:
    case 308:
    case 353:
    case 356:
    case 359: return 61;  // rain family
    case 185:
    case 281:
    case 284:
    case 311:
    case 314:
    case 317:
    case 320:
    case 350:
    case 362:
    case 365:
    case 374:
    case 377: return 67;  // freezing rain/sleet
    case 179:
    case 182:
    case 227:
    case 230:
    case 323:
    case 326:
    case 329:
    case 332:
    case 335:
    case 338:
    case 368:
    case 371:
    case 392:
    case 395: return 71;  // snow family
    case 200:
    case 386:
    case 389: return 95;  // storm
    default:
        return 3;
    }
}

static bool fetch_open_meteo_weather(float latValue, float lonValue, String &summary, String &tempText,
                                     String &windText, String &humidityText, int &weatherCodeOut, bool &isDayOut)
{
    String url = "http://wttr.in/" + String(latValue, 4) + "," + String(lonValue, 4) + "?format=j1";
    String payload;
    NetWorkerHttpOptions net_opts;
    net_opts.insecure_tls = false;
    net_opts.connect_timeout_ms = 5000;
    net_opts.timeout_ms = 7000;
    net_opts.disable_redirects = true;
    int httpCode = net_worker_http_get(url, &payload, &net_opts);
    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf("[weather] wttr failed: %d\n", httpCode);
        return false;
    }

    DynamicJsonDocument filter(384);
    filter["current_condition"][0]["temp_C"] = true;
    filter["current_condition"][0]["windspeedKmph"] = true;
    filter["current_condition"][0]["humidity"] = true;
    filter["current_condition"][0]["weatherCode"] = true;
    filter["current_condition"][0]["isdaytime"] = true;
    filter["current_condition"][0]["weatherDesc"][0]["value"] = true;

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (err)
    {
        Serial.printf("[weather] wttr json parse failed: %s\n", err.c_str());
        return false;
    }

    JsonObject current = doc["current_condition"][0];
    float tempC = current["temp_C"].as<float>();
    float windKmh = current["windspeedKmph"].as<float>();
    const float windMs = windKmh / 3.6f;
    int humidity = current["humidity"].as<int>();
    int weatherCode = current["weatherCode"].as<int>();
    bool isDay = true;
    if (current["isdaytime"].is<const char *>())
    {
        String dayFlag = current["isdaytime"].as<const char *>();
        dayFlag.toLowerCase();
        isDay = (dayFlag == "yes" || dayFlag == "day");
    }

    weatherCodeOut = map_wttr_code_to_visual_code(weatherCode);
    isDayOut = isDay;
    if (current["weatherDesc"][0]["value"].is<const char *>())
    {
        summary = current["weatherDesc"][0]["value"].as<const char *>();
    }
    else
    {
        summary = weather_code_to_text(weatherCode, isDay);
    }
    tempText = String((int)roundf(tempC));
    windText = String((int)roundf(windMs));
    humidityText = String(humidity);
    return true;
}

void getWeather(void *pvParameters)
{
    g_weather_task_handle = xTaskGetCurrentTaskHandle();
    Serial.println("[weather] task loop online");
    for (;;)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            if (!g_weather_ready && g_weather_summary.length() == 0)
            {
                g_weather_code = 3;
                g_weather_is_day = true;
                g_weather_summary = "Weather loading";
                g_weather_temp_text = "--";
                g_weather_wind_text = "--";
                g_weather_humidity_text = "--";
                g_weather_dirty = true;
            }
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            continue;
        }

        float latValue = 0.0f;
        float lonValue = 0.0f;
        String resolvedCity = "";
        String cityQuery = g_city_query;
        cityQuery.trim();

        if (kCityNameGeocodingEnabled && cityQuery.length() > 0)
        {
            if (getCityPositionByName(cityQuery, latValue, lonValue, resolvedCity))
            {
                g_city_name = resolvedCity.length() > 0 ? resolvedCity : cityQuery;
            }
        }

        if (latValue == 0.0f && lonValue == 0.0f && g_weather_saved_lat != 0.0f && g_weather_saved_lon != 0.0f)
        {
            latValue = g_weather_saved_lat;
            lonValue = g_weather_saved_lon;
            if (g_city_name.length() == 0)
            {
                g_city_name = "Kyiv";
            }
        }

        if (latValue == 0.0f && lonValue == 0.0f)
        {
            String ip = getIps();
            float *pos = getCityPosition(ip);
            latValue = pos[0];
            lonValue = pos[1];
            delete[] pos;
        }

        if (latValue == 0.0f && lonValue == 0.0f)
        {
            latValue = kWeatherDefaultLat;
            lonValue = kWeatherDefaultLon;
            if (g_city_name.length() == 0)
            {
                g_city_name = "Kyiv";
            }
        }

        lat = String(latValue, 6);
        lon = String(lonValue, 6);

        refresh_holiday_text(currentTime_year, currentTime_mouth, currentTime_day);

        String summary;
        String tempText;
        String windText;
        String humidityText;
        int weatherCode = -1;
        bool isDay = true;
        const bool weatherOk = fetch_open_meteo_weather(latValue, lonValue, summary, tempText, windText, humidityText, weatherCode, isDay);
        if (weatherOk)
        {
            Serial.printf("[weather] code=%d day=%d  %s  %s  %s  %s\n",
                          weatherCode, isDay ? 1 : 0, summary.c_str(), tempText.c_str(), windText.c_str(), humidityText.c_str());
            g_weather_saved_lat = latValue;
            g_weather_saved_lon = lonValue;
            if (g_prefs.begin("wifi", false))
            {
                g_prefs.putFloat("wlat", g_weather_saved_lat);
                g_prefs.putFloat("wlon", g_weather_saved_lon);
                g_prefs.end();
            }
            g_weather_ready = true;
            g_weather_code = weatherCode;
            g_weather_is_day = isDay;
            g_weather_summary = summary;
            g_weather_temp_text = tempText;
            g_weather_wind_text = windText;
            g_weather_humidity_text = humidityText;
            g_weather_dirty = true;
        }
        else if (!g_weather_ready)
        {
            Serial.println("[weather] fetch failed");
            g_weather_summary = "Weather unavailable";
            g_weather_temp_text = "--";
            g_weather_wind_text = "--";
            g_weather_humidity_text = "--";
            g_weather_dirty = true;
        }

        vTaskDelay((weatherOk ? (15UL * 60UL * 1000UL) : 60000UL) / portTICK_PERIOD_MS);
    }
    g_weather_task_handle = NULL;
    g_weather_task_started = false;
    vTaskDelete(NULL);
}

static bool fetch_weather_once_now()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    float latValue = 0.0f;
    float lonValue = 0.0f;
    String resolvedCity = "";
    String cityQuery = g_city_query;
    cityQuery.trim();

    if (kCityNameGeocodingEnabled && cityQuery.length() > 0)
    {
        if (getCityPositionByName(cityQuery, latValue, lonValue, resolvedCity))
        {
            g_city_name = resolvedCity.length() > 0 ? resolvedCity : cityQuery;
        }
    }

    if (latValue == 0.0f && lonValue == 0.0f && g_weather_saved_lat != 0.0f && g_weather_saved_lon != 0.0f)
    {
        latValue = g_weather_saved_lat;
        lonValue = g_weather_saved_lon;
        if (g_city_name.length() == 0)
        {
            g_city_name = "Kyiv";
        }
    }

    if (latValue == 0.0f && lonValue == 0.0f)
    {
        String ip = getIps();
        float *pos = getCityPosition(ip);
        latValue = pos[0];
        lonValue = pos[1];
        delete[] pos;
    }

    if (latValue == 0.0f && lonValue == 0.0f)
    {
        latValue = kWeatherDefaultLat;
        lonValue = kWeatherDefaultLon;
        if (g_city_name.length() == 0)
        {
            g_city_name = "Kyiv";
        }
    }

    String summary;
    String tempText;
    String windText;
    String humidityText;
    int weatherCode = -1;
    bool isDay = true;
    const bool ok = fetch_open_meteo_weather(latValue, lonValue, summary, tempText, windText, humidityText, weatherCode, isDay);
    if (!ok)
    {
        return false;
    }

    g_weather_saved_lat = latValue;
    g_weather_saved_lon = lonValue;
    if (g_prefs.begin("wifi", false))
    {
        g_prefs.putFloat("wlat", g_weather_saved_lat);
        g_prefs.putFloat("wlon", g_weather_saved_lon);
        g_prefs.end();
    }

    g_weather_ready = true;
    g_weather_code = weatherCode;
    g_weather_is_day = isDay;
    g_weather_summary = summary;
    g_weather_temp_text = tempText;
    g_weather_wind_text = windText;
    g_weather_humidity_text = humidityText;
    g_weather_dirty = true;
    return true;
}




















































































































































































































