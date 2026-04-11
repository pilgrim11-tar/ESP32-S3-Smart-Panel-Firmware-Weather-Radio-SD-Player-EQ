#include "radio_vendor.h"

#include <stdarg.h>
#include <stdio.h>

#include <SD.h>
#include <WiFi.h>

#if __has_include(<Audio.h>)
#define RADIO_VENDOR_BACKEND 1
#include <Audio.h>
#else
#define RADIO_VENDOR_BACKEND 0
#endif

static const VendorRadioStation kStations[] = {
    {"Hits", "http://live.powerhitz.com/hitlist?aw_0_req.gdpr=true"},
    {"Lounge", "http://ice5.somafm.com/illstreet-128-mp3"},
    {"Portugal", "http://radiocast.rtp.pt/antena380a.mp3"},
};

static uint8_t s_volume_percent = 60;
static int s_current_index = -1;
static bool s_playing = false;
static bool s_output_muted = false;
static uint8_t s_pinout_mode = 0;
static char s_last_status[96] = "Radio idle";
static char s_current_title[128] = "Nothing playing";
static char s_current_group[64] = "Idle";

static void copy_text(char *dst, size_t dst_size, const char *src, const char *fallback)
{
    if (!dst || dst_size == 0)
    {
        return;
    }
    const char *value = (src && src[0]) ? src : fallback;
    if (!value)
    {
        value = "";
    }
    snprintf(dst, dst_size, "%s", value);
}

void radio_vendor_set_media_info(const char *title, const char *group)
{
    copy_text(s_current_title, sizeof(s_current_title), title, "Nothing playing");
    copy_text(s_current_group, sizeof(s_current_group), group, "Idle");
}

static void set_media_info_from_path(const char *path)
{
    String fullPath = path ? String(path) : String();
    String title = fullPath;
    String group = "SD";

    int slash = fullPath.lastIndexOf('/');
    if (slash >= 0)
    {
        title = fullPath.substring(slash + 1);
        int prev = fullPath.lastIndexOf('/', slash - 1);
        if (prev >= 0)
        {
            group = fullPath.substring(prev + 1, slash);
        }
        else if (slash > 0)
        {
            group = fullPath.substring(0, slash);
        }
    }

    int dot = title.lastIndexOf('.');
    if (dot > 0)
    {
        title = title.substring(0, dot);
    }
    title.replace('_', ' ');
    group.replace('_', ' ');

    radio_vendor_set_media_info(title.c_str(), group.c_str());
}

static void set_status(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_last_status, sizeof(s_last_status), fmt, args);
    va_end(args);
    Serial.printf("[radio] %s\n", s_last_status);
}

#if RADIO_VENDOR_BACKEND
Audio audio;

struct AudioMessage
{
    uint8_t cmd;
    const char *txt;
    uint32_t value;
    uint32_t ret;
} audioTxMessage, audioRxMessage;

enum : uint8_t
{
    SET_VOLUME,
    GET_VOLUME,
    CONNECTTOHOST,
    CONNECTTOFS,
    STOPSONG
};

static QueueHandle_t audioSetQueue = NULL;
static QueueHandle_t audioGetQueue = NULL;
static bool s_audio_ready = false;
static uint8_t s_applied_volume_steps = 0;
static bool s_applied_volume_known = false;

static uint8_t volume_to_audio_steps(uint8_t percent)
{
    // Keep ~15% headroom to reduce clipping crackle on strong peaks.
    return (uint8_t)map(percent, 0, 100, 0, 18);
}

static uint8_t effective_volume_steps()
{
    return s_output_muted ? 0 : volume_to_audio_steps(s_volume_percent);
}

static AudioMessage transmitReceive(AudioMessage msg)
{
    if (!audioSetQueue || !audioGetQueue)
    {
        msg.ret = 0;
        return msg;
    }

    AudioMessage stale;
    while (xQueueReceive(audioGetQueue, &stale, 0) == pdPASS)
    {
    }

    if (xQueueSend(audioSetQueue, &msg, pdMS_TO_TICKS(100)) != pdPASS)
    {
        msg.ret = 0;
        set_status("Audio queue busy");
        return msg;
    }

    const TickType_t waitTicks = pdMS_TO_TICKS(msg.cmd == CONNECTTOHOST ? 6500 : 1200);
    if (xQueueReceive(audioGetQueue, &audioRxMessage, waitTicks) == pdPASS)
    {
        return audioRxMessage;
    }

    msg.ret = 0;
    set_status(msg.cmd == CONNECTTOHOST ? "Radio connect timeout" : "Audio command timeout");
    return msg;
}

static void apply_volume_steps_immediate(uint8_t steps)
{
    if (!s_audio_ready)
    {
        return;
    }

    AudioMessage msg = {};
    msg.cmd = SET_VOLUME;
    msg.value = (uint8_t)constrain(steps, 0, 21);
    transmitReceive(msg);
    s_applied_volume_steps = msg.value;
    s_applied_volume_known = true;
}

static void apply_volume_steps_smooth(uint8_t target_steps, uint8_t step_size = 2, uint8_t step_delay_ms = 5)
{
    if (!s_audio_ready)
    {
        return;
    }

    target_steps = (uint8_t)constrain(target_steps, 0, 21);
    uint8_t current = s_applied_volume_known ? s_applied_volume_steps : target_steps;
    if (!s_applied_volume_known)
    {
        apply_volume_steps_immediate(current);
    }

    if (current == target_steps)
    {
        return;
    }

    if (step_size == 0)
    {
        step_size = 1;
    }

    while (current != target_steps)
    {
        if (current < target_steps)
        {
            const uint8_t next = (uint8_t)min<int>((int)target_steps, (int)current + (int)step_size);
            apply_volume_steps_immediate(next);
            current = next;
        }
        else
        {
            const int next = max<int>((int)target_steps, (int)current - (int)step_size);
            apply_volume_steps_immediate((uint8_t)next);
            current = (uint8_t)next;
        }

        if (current != target_steps && step_delay_ms > 0)
        {
            delay(step_delay_ms);
        }
    }
}

static void soft_stop_playback()
{
    if (!s_audio_ready)
    {
        return;
    }

    if (s_playing)
    {
        apply_volume_steps_smooth(0, 2, 5);
    }

    AudioMessage msg = {};
    msg.cmd = STOPSONG;
    transmitReceive(msg);
    s_playing = false;
    s_current_index = -1;
}

static void CreateQueues()
{
    audioSetQueue = xQueueCreate(10, sizeof(struct AudioMessage));
    audioGetQueue = xQueueCreate(10, sizeof(struct AudioMessage));
}

static void audioTask(void *parameter)
{
    (void)parameter;
    CreateQueues();
    if (!audioSetQueue || !audioGetQueue)
    {
        set_status("Audio queue init failed");
        while (true)
        {
            vTaskDelay(portMAX_DELAY);
        }
    }

    AudioMessage rx;
    AudioMessage tx;

    for (;;)
    {
        if (xQueueReceive(audioSetQueue, &rx, 1) == pdPASS)
        {
            tx = {};
            tx.cmd = rx.cmd;

            if (rx.cmd == SET_VOLUME)
            {
                audio.setVolume(rx.value);
                tx.ret = 1;
            }
            else if (rx.cmd == CONNECTTOHOST)
            {
                tx.ret = audio.connecttohost(rx.txt);
            }
            else if (rx.cmd == CONNECTTOFS)
            {
                tx.ret = audio.connecttoFS(SD, rx.txt);
            }
            else if (rx.cmd == GET_VOLUME)
            {
                tx.ret = audio.getVolume();
            }
            else if (rx.cmd == STOPSONG)
            {
                tx.ret = audio.stopSong();
            }
            else
            {
                tx.ret = 0;
            }

            xQueueSend(audioGetQueue, &tx, portMAX_DELAY);
        }

        audio.loop();
    }
}

static bool ensure_audio_init()
{
    if (s_audio_ready)
    {
        return true;
    }

    if (xTaskCreatePinnedToCore(audioTask, "audioplay", 5000, NULL, 2 | portPRIVILEGE_BIT, NULL, 0) != pdPASS)
    {
        set_status("Audio task start failed");
        return false;
    }

    delay(50);
    // Give network streams a bit more headroom in PSRAM to reduce short dropouts.
    // Keep Wi-Fi stable on S3 by reducing startup audio buffers.
    audio.setBufsize(6000, 180000);
    audio.setConnectionTimeout(700, 5000);
    bool pinout_ok = false;
    if (s_pinout_mode == 1)
    {
        pinout_ok = audio.setPinout(2, 1, 40);
        Serial.println("[radio] pinout mode 1: BCLK=2 LRC=1 DOUT=40");
    }
    else
    {
        pinout_ok = audio.setPinout(1, 2, 40);
        Serial.println("[radio] pinout mode 0: BCLK=1 LRC=2 DOUT=40");
    }

    if (!pinout_ok)
    {
        set_status("Pinout setup failed");
        return false;
    }

    const uint8_t initialVolume = effective_volume_steps();
    audio.setVolume(initialVolume);
    s_applied_volume_steps = initialVolume;
    s_applied_volume_known = true;
    s_audio_ready = true;
    set_status("Audio backend ready");
    return true;
}

void audio_info(const char *info)
{
    if (info)
    {
        Serial.printf("[radio] info %s\n", info);
    }
}

void audio_id3data(const char *info)
{
    if (info)
    {
        Serial.printf("[radio] id3 %s\n", info);
    }
}

void audio_eof_mp3(const char *info)
{
    if (info)
    {
        Serial.printf("[radio] eof %s\n", info);
    }
}

void audio_showstation(const char *info)
{
    if (info)
    {
        Serial.printf("[radio] station %s\n", info);
    }

    if (s_current_index >= 0)
    {
        copy_text(s_current_group, sizeof(s_current_group), kStations[s_current_index].name, "Radio");
        set_status("Playing %s", kStations[s_current_index].name);
    }
    else if (info)
    {
        set_status("%s", info);
    }
}

void audio_showstreamtitle(const char *info)
{
    if (info)
    {
        Serial.printf("[radio] title %s\n", info);
        copy_text(s_current_title, sizeof(s_current_title), info, s_current_title);
    }
}

void audio_bitrate(const char *info)
{
    if (info)
    {
        Serial.printf("[radio] bitrate %s\n", info);
    }
}

void audio_commercial(const char *info)
{
    if (info)
    {
        Serial.printf("[radio] commercial %s\n", info);
    }
}

void audio_icyurl(const char *info)
{
    if (info)
    {
        Serial.printf("[radio] icyurl %s\n", info);
    }
}

void audio_lasthost(const char *info)
{
    if (info)
    {
        Serial.printf("[radio] host %s\n", info);
    }
}
#endif

const VendorRadioStation *radio_vendor_stations(size_t *count)
{
    if (count)
    {
        *count = sizeof(kStations) / sizeof(kStations[0]);
    }
    return kStations;
}

void radio_vendor_init()
{
    set_status("Radio idle");
    radio_vendor_set_media_info("Nothing playing", "Idle");
#if RADIO_VENDOR_BACKEND
    ensure_audio_init();
#endif
}

void radio_vendor_loop()
{
#if RADIO_VENDOR_BACKEND
    if (s_audio_ready)
    {
        s_playing = audio.isRunning();
    }
#endif
}

void radio_vendor_stop()
{
#if RADIO_VENDOR_BACKEND
    if (s_audio_ready)
    {
        soft_stop_playback();
    }
#endif
    s_playing = false;
    set_status("Radio stopped");
}

bool radio_vendor_start_index(int idx)
{
    size_t count = 0;
    radio_vendor_stations(&count);
    if (idx < 0 || idx >= (int)count)
    {
        return false;
    }

#if RADIO_VENDOR_BACKEND
    if (WiFi.status() != WL_CONNECTED)
    {
        set_status("WiFi is offline");
        return false;
    }
    if (!ensure_audio_init())
    {
        return false;
    }

    const uint8_t targetVolume = effective_volume_steps();
    if (s_playing)
    {
        soft_stop_playback();
        delay(12);
    }
    else
    {
        apply_volume_steps_immediate(0);
    }

    AudioMessage msg = {};
    msg.cmd = CONNECTTOHOST;
    msg.txt = kStations[idx].url;
    set_status("Connecting %s", kStations[idx].name);
    AudioMessage rx = transmitReceive(msg);
    if (!rx.ret)
    {
        s_playing = false;
        set_status("Host connect failed");
        return false;
    }
#endif

    s_current_index = idx;
    s_playing = true;
    radio_vendor_set_media_info(kStations[idx].name, "Radio");
    set_status("Playing %s", kStations[idx].name);
    apply_volume_steps_smooth(targetVolume, 2, 5);
    return true;
}

bool radio_vendor_start_random()
{
    size_t count = 0;
    radio_vendor_stations(&count);
    if (count == 0)
    {
        return false;
    }

    const int start = random((int)count);
    for (size_t attempt = 0; attempt < count; ++attempt)
    {
        const int idx = (start + (int)attempt) % (int)count;
        if (radio_vendor_start_index(idx))
        {
            return true;
        }
    }
    return false;
}

bool radio_vendor_next()
{
    size_t count = 0;
    radio_vendor_stations(&count);
    if (count == 0)
    {
        return false;
    }
    int next = s_current_index < 0 ? 0 : (s_current_index + 1) % (int)count;
    return radio_vendor_start_index(next);
}

bool radio_vendor_is_playing()
{
    return s_playing;
}

int radio_vendor_current_index()
{
    return s_current_index;
}

uint8_t radio_vendor_get_volume_percent()
{
    return s_volume_percent;
}

void radio_vendor_set_volume_percent(uint8_t percent)
{
    s_volume_percent = (uint8_t)constrain(percent, 0, 100);
#if RADIO_VENDOR_BACKEND
    if (s_audio_ready)
    {
        const uint8_t targetVolume = effective_volume_steps();
        if (s_playing)
        {
            apply_volume_steps_smooth(targetVolume, 2, 3);
        }
        else
        {
            apply_volume_steps_immediate(targetVolume);
        }
    }
#endif
}

void radio_vendor_set_output_muted(bool muted)
{
    s_output_muted = muted;
#if RADIO_VENDOR_BACKEND
    if (s_audio_ready)
    {
        const uint8_t targetVolume = effective_volume_steps();
        if (s_playing)
        {
            apply_volume_steps_smooth(targetVolume, 2, 3);
        }
        else
        {
            apply_volume_steps_immediate(targetVolume);
        }
    }
#endif
}

bool radio_vendor_output_muted()
{
    return s_output_muted;
}

const char *radio_vendor_current_name()
{
    size_t count = 0;
    radio_vendor_stations(&count);
    if (s_current_index < 0 || s_current_index >= (int)count)
    {
        return "Idle";
    }
    return kStations[s_current_index].name;
}

bool radio_vendor_play_test()
{
    return false;
}

bool radio_vendor_play_speech(const char *text)
{
    (void)text;
    set_status("Speech disabled");
    return false;
}

static bool radio_vendor_play_sd_internal(const char *path, const char *title, const char *group)
{
#if RADIO_VENDOR_BACKEND
    if (!path || !ensure_audio_init())
    {
        return false;
    }

    const uint8_t targetVolume = effective_volume_steps();
    if (s_playing)
    {
        soft_stop_playback();
        delay(12);
    }
    apply_volume_steps_immediate(0);

    AudioMessage msg = {};
    msg.cmd = CONNECTTOFS;
    msg.txt = path;
    if ((title && title[0]) || (group && group[0]))
    {
        radio_vendor_set_media_info(title, group);
    }
    else
    {
        set_media_info_from_path(path);
    }
    set_status("Playing %s", path);
    AudioMessage rx = transmitReceive(msg);
    if (!rx.ret)
    {
        s_playing = false;
        set_status("SD play failed");
        return false;
    }

    s_current_index = -1;
    s_playing = true;
    apply_volume_steps_smooth(targetVolume, 2, 5);
    return true;
#else
    (void)path;
    (void)title;
    (void)group;
    return false;
#endif
}

bool radio_vendor_play_sd(const char *path)
{
    return radio_vendor_play_sd_internal(path, nullptr, nullptr);
}

bool radio_vendor_play_sd_with_info(const char *path, const char *title, const char *group)
{
    return radio_vendor_play_sd_internal(path, title, group);
}

void radio_vendor_set_pinout_mode(uint8_t mode)
{
    s_pinout_mode = mode > 0 ? 1 : 0;
}

uint8_t radio_vendor_get_pinout_mode()
{
    return s_pinout_mode;
}

bool radio_vendor_backend_ready()
{
#if RADIO_VENDOR_BACKEND
    return s_audio_ready;
#else
    return false;
#endif
}

const char *radio_vendor_last_status()
{
    return s_last_status;
}

uint16_t radio_vendor_get_vu_level()
{
    return 0;
}










const char *radio_vendor_current_title()
{
    return s_current_title;
}

const char *radio_vendor_current_group()
{
    return s_current_group;
}







