#include "audio_visualizer.h"

namespace
{
constexpr uint8_t kBarCount = 8;
constexpr lv_coord_t kContainerWidth = 168;
constexpr lv_coord_t kContainerHeight = 44;
constexpr lv_coord_t kBarWidth = 12;
constexpr lv_coord_t kBarGap = 8;
constexpr lv_coord_t kBarMinHeight = 4;
constexpr lv_coord_t kBarMaxHeight = 34;
constexpr uint32_t kDecayAfterMs = 260;
constexpr uint32_t kUiUpdateMs = 33;

volatile uint8_t s_left_level = 0;
volatile uint8_t s_right_level = 0;
volatile uint32_t s_last_signal_ms = 0;
volatile uint32_t s_dynamic_peak = 4096;

uint8_t s_target_levels[kBarCount] = {};
uint8_t s_ui_levels[kBarCount] = {};
uint32_t s_last_ui_ms = 0;

lv_obj_t *s_container = nullptr;
lv_obj_t *s_bars[kBarCount] = {};

uint32_t abs16(int16_t v)
{
    return v < 0 ? static_cast<uint32_t>(-static_cast<int32_t>(v)) : static_cast<uint32_t>(v);
}

uint8_t scale_to_percent(uint32_t peak)
{
    uint32_t dyn = s_dynamic_peak;
    if (peak > dyn)
    {
        dyn = peak;
    }
    else
    {
        const uint32_t decay = dyn / 32U;
        dyn = (dyn > decay + 4096U) ? (dyn - decay) : 4096U;
        if (peak > dyn)
        {
            dyn = peak;
        }
    }
    if (dyn < 4096U)
    {
        dyn = 4096U;
    }
    s_dynamic_peak = dyn;

    uint32_t pct = (peak * 100U) / dyn;
    if (pct > 100U)
    {
        pct = 100U;
    }
    return static_cast<uint8_t>(pct);
}

void apply_bar_style(lv_obj_t *bar, uint8_t index)
{
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, kBarWidth, kBarMinHeight);
    lv_obj_set_style_radius(bar, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_color_t color = index < 3 ? lv_color_hex(0x89D8FF) :
                       index < 6 ? lv_color_hex(0xA8F2FF) :
                                   lv_color_hex(0xD7F8FF);
    lv_obj_set_style_bg_color(bar, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_80, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}
} // namespace

void audio_visualizer_init()
{
    for (uint8_t i = 0; i < kBarCount; ++i)
    {
        s_target_levels[i] = 0;
        s_ui_levels[i] = 0;
    }
    s_left_level = 0;
    s_right_level = 0;
    s_last_signal_ms = 0;
    s_dynamic_peak = 4096;
    s_last_ui_ms = 0;
}

void audio_visualizer_attach(lv_obj_t *parent)
{
    if (!parent || s_container)
    {
        return;
    }

    s_container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_container);
    lv_obj_set_size(s_container, kContainerWidth, kContainerHeight);
    lv_obj_align(s_container, LV_ALIGN_BOTTOM_MID, 0, -42);
    lv_obj_set_style_bg_opa(s_container, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(s_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(s_container, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_move_foreground(s_container);

    const lv_coord_t used_width = (kBarCount * kBarWidth) + ((kBarCount - 1) * kBarGap);
    const lv_coord_t start_x = (kContainerWidth - used_width) / 2;

    for (uint8_t i = 0; i < kBarCount; ++i)
    {
        s_bars[i] = lv_obj_create(s_container);
        apply_bar_style(s_bars[i], i);
        lv_obj_set_pos(s_bars[i], start_x + i * (kBarWidth + kBarGap), kContainerHeight - kBarMinHeight);
    }
}

void audio_visualizer_update_ui()
{
    if (!s_container)
    {
        return;
    }

    const uint32_t now = millis();
    if (now - s_last_ui_ms < kUiUpdateMs)
    {
        return;
    }
    s_last_ui_ms = now;

    lv_obj_t *screen = lv_obj_get_parent(s_container);
    if (screen && lv_scr_act() != screen)
    {
        lv_obj_add_flag(s_container, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_container, LV_OBJ_FLAG_HIDDEN);

    if (s_last_signal_ms != 0 && (now - s_last_signal_ms) <= kDecayAfterMs)
    {
        const uint8_t left = s_left_level;
        const uint8_t right = s_right_level;
        s_target_levels[0] = left / 3;
        s_target_levels[1] = left / 2;
        s_target_levels[2] = (left * 3) / 4;
        s_target_levels[3] = left;
        s_target_levels[4] = right;
        s_target_levels[5] = (right * 3) / 4;
        s_target_levels[6] = right / 2;
        s_target_levels[7] = right / 3;
    }
    else
    {
        for (uint8_t i = 0; i < kBarCount; ++i)
        {
            s_target_levels[i] = 0;
        }
    }

    for (uint8_t i = 0; i < kBarCount; ++i)
    {
        const uint8_t target = s_target_levels[i];
        uint8_t current = s_ui_levels[i];
        if (current < target)
        {
            current = static_cast<uint8_t>(min<int>(target, current + max<int>(6, target - current)));
        }
        else if (current > target)
        {
            current = static_cast<uint8_t>(max<int>(target, current - max<int>(3, (current - target + 1) / 2)));
        }
        if (current == s_ui_levels[i])
        {
            continue;
        }
        s_ui_levels[i] = current;

        const lv_coord_t height = kBarMinHeight + (current * (kBarMaxHeight - kBarMinHeight)) / 100;
        lv_obj_set_size(s_bars[i], kBarWidth, height);
        lv_obj_set_y(s_bars[i], kContainerHeight - height);
        lv_obj_set_style_bg_opa(s_bars[i], static_cast<lv_opa_t>(LV_OPA_50 + (current * (LV_OPA_90 - LV_OPA_50)) / 100), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void audio_process_extern(int16_t *buff, uint16_t len, bool *continueI2S)
{
    if (continueI2S)
    {
        *continueI2S = true;
    }

    if (!buff || len == 0)
    {
        return;
    }

    // Audio callback data is interleaved stereo (L,R,...). Keep strict bounds.
    const uint16_t frames = len / 2U;
    if (frames == 0)
    {
        return;
    }

    uint32_t left_peak = 0;
    uint32_t right_peak = 0;
    const uint16_t step = max<uint16_t>(1, frames / 64U);
    for (uint16_t i = 0; i < frames; i += step)
    {
        const uint16_t base = (uint16_t)(i * 2U);
        const uint32_t left = abs16(buff[base]);
        const uint32_t right = abs16(buff[base + 1U]);
        if (left > left_peak) left_peak = left;
        if (right > right_peak) right_peak = right;
    }

    s_left_level = scale_to_percent(left_peak);
    s_right_level = scale_to_percent(right_peak);
    s_last_signal_ms = millis();
}


bool audio_visualizer_copy_levels(uint8_t *out_levels, size_t count)
{
    if (!out_levels || count == 0)
    {
        return false;
    }

    const uint32_t now = millis();
    const bool fresh = s_last_signal_ms != 0 && (now - s_last_signal_ms) <= kDecayAfterMs;

    uint8_t snapshot[kBarCount] = {};
    if (fresh)
    {
        const uint8_t left = s_left_level;
        const uint8_t right = s_right_level;
        snapshot[0] = left / 3;
        snapshot[1] = left / 2;
        snapshot[2] = (left * 3) / 4;
        snapshot[3] = left;
        snapshot[4] = right;
        snapshot[5] = (right * 3) / 4;
        snapshot[6] = right / 2;
        snapshot[7] = right / 3;
    }

    const size_t n = min<size_t>(count, kBarCount);
    for (size_t i = 0; i < n; ++i)
    {
        out_levels[i] = snapshot[i];
    }
    for (size_t i = n; i < count; ++i)
    {
        out_levels[i] = 0;
    }

    return fresh;
}

