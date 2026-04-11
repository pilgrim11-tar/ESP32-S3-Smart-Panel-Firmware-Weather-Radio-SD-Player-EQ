#include "weather_sd_assets.h"

#include "radio_vendor.h"
#include "sd_vendor.h"
#include "serial_upload_port.h"

#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

namespace
{
HardwareSerial &kUploadSerial = UploadSerial;

constexpr size_t kCmdBufferSize = 160;
constexpr size_t kIoChunkSize = 1024;
constexpr uint8_t kMaxFramesPerState = 32;
constexpr uint16_t kMaxWeatherFrameDim = 512;

struct WeatherStateInfo
{
    const char *name;
    uint8_t frame_count;
};

struct FrameSlot
{
    lv_img_dsc_t dsc{};
    uint8_t *data = nullptr;
    size_t capacity = 0;
    char state[24]{};
    uint8_t frame = 0xFF;
    bool valid = false;
};

struct UploadState
{
    bool active = false;
    File file;
    size_t remaining = 0;
    char path[96]{};
};

WeatherStateInfo s_states[] = {
    {"clear", 0},
    {"clear_night", 0},
    {"partly_cloudy", 0},
    {"partly_cloudy_night", 0},
    {"cloudy", 0},
    {"fog", 0},
    {"rain", 0},
    {"storm", 0},
    {"snow", 0},
};

FrameSlot s_slots[2];
uint8_t s_active_slot = 0;
char s_cmd_buffer[kCmdBufferSize]{};
size_t s_cmd_len = 0;
UploadState s_upload;
uint32_t s_last_serial_activity_ms = 0;

WeatherStateInfo *find_state(const char *name)
{
    for (auto &state : s_states)
    {
        if (strcmp(state.name, name) == 0)
        {
            return &state;
        }
    }
    return nullptr;
}

void free_slot(FrameSlot &slot)
{
    if (slot.data)
    {
        heap_caps_free(slot.data);
        slot.data = nullptr;
    }
    slot.capacity = 0;
    slot.valid = false;
    slot.state[0] = '\0';
    slot.frame = 0xFF;
}

bool ensure_slot_capacity(FrameSlot &slot, size_t needed)
{
    if (slot.capacity >= needed && slot.data)
    {
        return true;
    }

    free_slot(slot);
    slot.data = static_cast<uint8_t *>(heap_caps_malloc(needed, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!slot.data)
    {
        slot.data = static_cast<uint8_t *>(malloc(needed));
    }
    if (!slot.data)
    {
        return false;
    }

    slot.capacity = needed;
    return true;
}

bool file_exists(const char *path)
{
    return SD.exists(path);
}

void rescan_states()
{
    if (!sd_vendor_ready())
    {
        for (auto &state : s_states)
        {
            state.frame_count = 0;
        }
        return;
    }

    for (auto &state : s_states)
    {
        uint8_t count = 0;
        char path[96];
        for (uint8_t i = 0; i < kMaxFramesPerState; ++i)
        {
            snprintf(path, sizeof(path), "/weather/%s/frame_%02u.bin", state.name, i);
            if (!file_exists(path))
            {
                break;
            }
            count++;
        }
        state.frame_count = count;
        Serial.printf("[weather-sd] %s frames=%u\n", state.name, count);
    }
}

bool mkdirs_for_path(const char *path)
{
    if (!path || path[0] != '/')
    {
        return false;
    }

    char tmp[96];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    for (char *p = tmp + 1; *p; ++p)
    {
        if (*p == '/')
        {
            *p = '\0';
            SD.mkdir(tmp);
            *p = '/';
        }
    }
    return true;
}

void finish_upload(bool ok, const char *message)
{
    if (s_upload.file)
    {
        s_upload.file.flush();
        s_upload.file.close();
    }
    s_upload.active = false;
    s_upload.remaining = 0;
    s_upload.path[0] = '\0';
    kUploadSerial.println(ok ? "WX_DONE" : message);
}

void handle_command(const char *line)
{
    s_last_serial_activity_ms = millis();
    if (strcmp(line, "WX_PING") == 0)
    {
        sd_vendor_ensure_ready();
        radio_vendor_stop();
        kUploadSerial.println("WX_PONG");
        return;
    }

    if (strcmp(line, "WX_END") == 0)
    {
        rescan_states();
        kUploadSerial.println("WX_READY");
        return;
    }

    if (strncmp(line, "WX_PUT ", 7) == 0)
    {
        if (!sd_vendor_ensure_ready())
        {
            kUploadSerial.println("WX_ERR no-sd");
            return;
        }

        const char *payload = line + 7;
        const char *space = strrchr(payload, ' ');
        if (!space)
        {
            Serial.println("WX_ERR bad-put");
            return;
        }

        char path[96];
        const size_t path_len = static_cast<size_t>(space - payload);
        if (path_len == 0 || path_len >= sizeof(path))
        {
            Serial.println("WX_ERR bad-path");
            return;
        }

        memcpy(path, payload, path_len);
        path[path_len] = '\0';
        const size_t size = static_cast<size_t>(strtoul(space + 1, nullptr, 10));

        radio_vendor_stop();
        mkdirs_for_path(path);
        if (SD.exists(path))
        {
            SD.remove(path);
        }

        File file = SD.open(path, FILE_WRITE);
        if (!file)
        {
            kUploadSerial.println("WX_ERR open");
            return;
        }

        s_upload.file = file;
        s_upload.active = true;
        s_upload.remaining = size;
        strncpy(s_upload.path, path, sizeof(s_upload.path) - 1);
        s_upload.path[sizeof(s_upload.path) - 1] = '\0';
        kUploadSerial.println("WX_OK");
        return;
    }

    if (line[0] != '\0')
    {
        kUploadSerial.printf("[weather-sd] ignored command: %s\n", line);
    }
}
} // namespace

void weather_sd_assets_init()
{
    rescan_states();
}

bool weather_sd_assets_session_active()
{
    if (s_upload.active)
    {
        return true;
    }
    if (s_last_serial_activity_ms == 0)
    {
        return false;
    }
    return (millis() - s_last_serial_activity_ms) < 15000UL;
}

bool weather_sd_assets_has_animation(const char *state_name)
{
    WeatherStateInfo *state = find_state(state_name);
    if (state && state->frame_count == 0 && sd_vendor_ready())
    {
        rescan_states();
        state = find_state(state_name);
    }
    return state && state->frame_count > 0;
}

uint8_t weather_sd_assets_frame_count(const char *state_name)
{
    WeatherStateInfo *state = find_state(state_name);
    if (state && state->frame_count == 0 && sd_vendor_ready())
    {
        rescan_states();
        state = find_state(state_name);
    }
    return state ? state->frame_count : 0;
}

bool weather_sd_assets_load_frame(const char *state_name, uint8_t frame_index, const lv_img_dsc_t **out_dsc)
{
    if (!out_dsc || !state_name || !sd_vendor_ready())
    {
        return false;
    }

    WeatherStateInfo *state = find_state(state_name);
    if (state && state->frame_count == 0)
    {
        rescan_states();
        state = find_state(state_name);
    }
    if (!state || state->frame_count == 0)
    {
        return false;
    }

    frame_index = static_cast<uint8_t>(frame_index % state->frame_count);

    FrameSlot &active = s_slots[s_active_slot];
    if (active.valid && active.frame == frame_index && strcmp(active.state, state_name) == 0)
    {
        *out_dsc = &active.dsc;
        return true;
    }

    char path[96];
    snprintf(path, sizeof(path), "/weather/%s/frame_%02u.bin", state_name, frame_index);

    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        return false;
    }

    const size_t file_size = static_cast<size_t>(file.size());
    if (file_size <= sizeof(lv_img_header_t))
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
    if (header.w == 0 || header.h == 0 || header.w > kMaxWeatherFrameDim || header.h > kMaxWeatherFrameDim)
    {
        Serial.printf("[weather-sd] reject frame %s w=%u h=%u\n", path, (unsigned)header.w, (unsigned)header.h);
        file.close();
        return false;
    }

    const size_t payload = file_size - sizeof(header);
    const uint8_t next_slot_index = static_cast<uint8_t>((s_active_slot + 1) % 2);
    FrameSlot &slot = s_slots[next_slot_index];
    if (!ensure_slot_capacity(slot, payload))
    {
        file.close();
        return false;
    }

    if (file.read(slot.data, payload) != static_cast<int>(payload))
    {
        file.close();
        return false;
    }
    file.close();

    slot.dsc.header = header;
    slot.dsc.data_size = payload;
    slot.dsc.data = slot.data;
    strncpy(slot.state, state_name, sizeof(slot.state) - 1);
    slot.state[sizeof(slot.state) - 1] = '\0';
    slot.frame = frame_index;
    slot.valid = true;
    s_active_slot = next_slot_index;

    *out_dsc = &slot.dsc;
    return true;
}

void weather_sd_assets_poll_upload()
{
    if (s_upload.active)
    {
        uint8_t buf[kIoChunkSize];
        while (s_upload.remaining > 0 && kUploadSerial.available() > 0)
        {
            s_last_serial_activity_ms = millis();
            const size_t chunk = min(static_cast<size_t>(kUploadSerial.available()), min(sizeof(buf), s_upload.remaining));
            const size_t read = static_cast<size_t>(kUploadSerial.readBytes(reinterpret_cast<char *>(buf), chunk));
            if (read == 0)
            {
                break;
            }
            if (s_upload.file.write(buf, read) != read)
            {
                finish_upload(false, "WX_ERR write");
                return;
            }
            s_upload.remaining -= read;
        }

        if (s_upload.remaining == 0)
        {
            finish_upload(true, "WX_DONE");
        }
        return;
    }

    while (kUploadSerial.available() > 0)
    {
        const int raw = kUploadSerial.read();
        if (raw < 0)
        {
            break;
        }

        const char ch = static_cast<char>(raw);
        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            s_cmd_buffer[s_cmd_len] = '\0';
            handle_command(s_cmd_buffer);
            s_cmd_len = 0;
            continue;
        }

        if (s_cmd_len + 1 < sizeof(s_cmd_buffer))
        {
            s_cmd_buffer[s_cmd_len++] = ch;
        }
    }
}

