#include "home_bg_sd_assets.h"

#include "sd_vendor.h"
#include "weather_sd_assets.h"

#include <SD.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

namespace
{
struct BgSlot
{
    lv_img_dsc_t dsc{};
    uint8_t *data = nullptr;
    size_t capacity = 0;
    char key[32]{};
    bool valid = false;
};

BgSlot s_slot;

const char *season_name(uint8_t month)
{
    switch (month)
    {
    case 3:
    case 4:
    case 5:
        return "spring";
    case 6:
    case 7:
    case 8:
        return "summer";
    case 9:
    case 10:
    case 11:
        return "autumn";
    default:
        return "winter";
    }
}

const char *daypart_name(uint8_t hour)
{
    if (hour >= 5 && hour <= 10)
    {
        return "morning";
    }
    if (hour >= 11 && hour <= 17)
    {
        return "day";
    }
    return "night";
}

void free_slot()
{
    if (s_slot.data)
    {
        heap_caps_free(s_slot.data);
        s_slot.data = nullptr;
    }
    s_slot.capacity = 0;
    s_slot.valid = false;
    s_slot.key[0] = '\0';
}

bool ensure_capacity(size_t needed)
{
    if (s_slot.data && s_slot.capacity >= needed)
    {
        return true;
    }

    free_slot();
    s_slot.data = static_cast<uint8_t *>(heap_caps_malloc(needed, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!s_slot.data)
    {
        s_slot.data = static_cast<uint8_t *>(malloc(needed));
    }
    if (!s_slot.data)
    {
        return false;
    }
    s_slot.capacity = needed;
    return true;
}

bool load_key(const char *key)
{
    if (!key || !sd_vendor_ready())
    {
        return false;
    }

    if (s_slot.valid && strcmp(s_slot.key, key) == 0)
    {
        return true;
    }

    char path[96];
    snprintf(path, sizeof(path), "/home_bg/%s.bin", key);
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

    const size_t payload = file_size - sizeof(header);
    if (!ensure_capacity(payload))
    {
        file.close();
        return false;
    }

    if (file.read(s_slot.data, payload) != static_cast<int>(payload))
    {
        file.close();
        return false;
    }
    file.close();

    s_slot.dsc.header = header;
    s_slot.dsc.data_size = payload;
    s_slot.dsc.data = s_slot.data;
    strncpy(s_slot.key, key, sizeof(s_slot.key) - 1);
    s_slot.key[sizeof(s_slot.key) - 1] = '\0';
    s_slot.valid = true;
    return true;
}
} // namespace

void home_bg_sd_assets_init()
{
}

String home_bg_sd_assets_key_for(uint8_t month, uint8_t hour)
{
    if (hour < 5)
    {
        return "";
    }
    return String(season_name(month)) + "_" + daypart_name(hour);
}

bool home_bg_sd_assets_apply(lv_obj_t *image_obj, uint8_t month, uint8_t hour)
{
    if (!image_obj)
    {
        return false;
    }

    const String key = home_bg_sd_assets_key_for(month, hour);
    if (key.length() == 0)
    {
        return false;
    }
    if (!load_key(key.c_str()))
    {
        return false;
    }

    lv_img_set_src(image_obj, &s_slot.dsc);
    return true;
}
