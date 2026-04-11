#line 1 "C:\\Users\\Taras\\Documents\\mou\\86switch_onoff_ai\\weather_visuals.cpp"
#include "weather_visuals.h"

#include "sd_vendor.h"
#include "ui.h"
#include "weather_sd_assets.h"
#include "weather_anim_frames.h"
#include "weather_anim_night_frames.h"

namespace
{
lv_obj_t *s_weather_overlay_img = nullptr;

constexpr lv_coord_t kSdPrimaryX = 61;
constexpr lv_coord_t kSdPrimaryY = -136;
constexpr uint16_t kSdPrimaryZoom = 256;
constexpr uint16_t kSdTargetMaxPx = 56;
constexpr uint16_t kWeatherZoomMin = 256;
constexpr uint16_t kWeatherZoomMax = 256;

uint16_t weather_zoom_scaled(uint16_t zoom)
{
    const uint32_t scaled = (uint32_t)zoom;
    if (scaled < kWeatherZoomMin) {
        return kWeatherZoomMin;
    }
    if (scaled > kWeatherZoomMax) {
        return kWeatherZoomMax;
    }
    return (uint16_t)scaled;
}

lv_obj_t *ensure_overlay_img()
{
    if (!s_weather_overlay_img && ui_Screen1)
    {
        s_weather_overlay_img = lv_img_create(ui_Screen1);
        lv_obj_set_width(s_weather_overlay_img, LV_SIZE_CONTENT);
        lv_obj_set_height(s_weather_overlay_img, LV_SIZE_CONTENT);
        lv_obj_set_align(s_weather_overlay_img, LV_ALIGN_CENTER);
        lv_obj_add_flag(s_weather_overlay_img, LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_clear_flag(s_weather_overlay_img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(s_weather_overlay_img, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_move_foreground(s_weather_overlay_img);
        lv_obj_add_flag(s_weather_overlay_img, LV_OBJ_FLAG_HIDDEN);
    }
    return s_weather_overlay_img;
}

void hide_overlay_img()
{
    if (s_weather_overlay_img)
    {
        lv_obj_add_flag(s_weather_overlay_img, LV_OBJ_FLAG_HIDDEN);
    }
}

void apply_sd_frame(lv_obj_t *img, const lv_img_dsc_t *frame, lv_coord_t x, lv_coord_t y, uint16_t zoom)
{
    if (!frame) {
        return;
    }
    uint16_t resolved_zoom = zoom;
    const uint16_t fw = frame->header.w;
    const uint16_t fh = frame->header.h;
    const uint16_t max_dim = (fw > fh) ? fw : fh;
    if (max_dim > kSdTargetMaxPx) {
        const uint32_t fit = ((uint32_t)kSdTargetMaxPx * 256U) / (uint32_t)max_dim;
        resolved_zoom = (uint16_t)((fit < 64U) ? 64U : fit);
    }
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_x(img, x);
    lv_obj_set_y(img, y);
    lv_img_set_zoom(img, weather_zoom_scaled(resolved_zoom));
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_img_set_src(img, frame);
}

void apply_static_visual(lv_obj_t *img, lv_color_t recolor, lv_opa_t recolor_opa, lv_coord_t x, lv_coord_t y, uint16_t zoom)
{
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_x(img, x);
    lv_obj_set_y(img, y);
    lv_img_set_zoom(img, weather_zoom_scaled(zoom));
    lv_obj_set_style_img_recolor(img, recolor, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(img, recolor_opa, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_img_set_src(img, &ui_img_s1_sunny_png);
}
} // namespace

static const lv_img_dsc_t *kWeatherClearFrames[] = {
    &ui_img_weather_clear_00,
    &ui_img_weather_clear_01,
    &ui_img_weather_clear_02,
    &ui_img_weather_clear_03,
    &ui_img_weather_clear_04,
    &ui_img_weather_clear_05,
    &ui_img_weather_clear_06,
    &ui_img_weather_clear_07,
    &ui_img_weather_clear_08,
    &ui_img_weather_clear_09,
    &ui_img_weather_clear_10,
    &ui_img_weather_clear_11,
};

static const lv_img_dsc_t *kWeatherNightFrames[] = {
    &ui_img_weather_night_00,
    &ui_img_weather_night_01,
    &ui_img_weather_night_02,
    &ui_img_weather_night_03,
    &ui_img_weather_night_04,
    &ui_img_weather_night_05,
    &ui_img_weather_night_06,
    &ui_img_weather_night_07,
    &ui_img_weather_night_08,
    &ui_img_weather_night_09,
    &ui_img_weather_night_10,
    &ui_img_weather_night_11,
};

enum WeatherVisualKind {
    WEATHER_VISUAL_STATIC = 0,
    WEATHER_VISUAL_CLEAR_DAY,
    WEATHER_VISUAL_CLEAR_NIGHT,
    WEATHER_VISUAL_PARTLY_CLOUDY_DAY,
    WEATHER_VISUAL_PARTLY_CLOUDY_NIGHT,
    WEATHER_VISUAL_CLOUDY,
    WEATHER_VISUAL_FOG,
    WEATHER_VISUAL_RAIN,
    WEATHER_VISUAL_STORM,
    WEATHER_VISUAL_SNOW,
};

static WeatherVisualKind weather_visual_kind_from_code(int weather_code, bool is_day)
{
    if (weather_code == 0) {
        return is_day ? WEATHER_VISUAL_CLEAR_DAY : WEATHER_VISUAL_CLEAR_NIGHT;
    }
    if (weather_code == 1 || weather_code == 2) {
        return is_day ? WEATHER_VISUAL_PARTLY_CLOUDY_DAY : WEATHER_VISUAL_PARTLY_CLOUDY_NIGHT;
    }
    if (weather_code == 3) {
        return WEATHER_VISUAL_CLOUDY;
    }
    if (weather_code == 45 || weather_code == 48) {
        return WEATHER_VISUAL_FOG;
    }
    if ((weather_code >= 51 && weather_code <= 67) || (weather_code >= 80 && weather_code <= 82)) {
        return WEATHER_VISUAL_RAIN;
    }
    if ((weather_code >= 71 && weather_code <= 77) || weather_code == 85 || weather_code == 86) {
        return WEATHER_VISUAL_SNOW;
    }
    if (weather_code == 95 || weather_code == 96 || weather_code == 99) {
        return WEATHER_VISUAL_STORM;
    }
    return WEATHER_VISUAL_STATIC;
}

static const char *weather_sd_state_name(WeatherVisualKind kind)
{
    switch (kind) {
    case WEATHER_VISUAL_CLEAR_DAY: return "clear";
    case WEATHER_VISUAL_CLEAR_NIGHT: return "clear_night";
    case WEATHER_VISUAL_PARTLY_CLOUDY_DAY: return "partly_cloudy";
    case WEATHER_VISUAL_PARTLY_CLOUDY_NIGHT: return "partly_cloudy_night";
    case WEATHER_VISUAL_CLOUDY: return "cloudy";
    case WEATHER_VISUAL_FOG: return "fog";
    case WEATHER_VISUAL_RAIN: return "rain";
    case WEATHER_VISUAL_STORM: return "storm";
    case WEATHER_VISUAL_SNOW: return "snow";
    case WEATHER_VISUAL_STATIC:
    default:
        return nullptr;
    }
}

void weather_visuals_init()
{
    weather_sd_assets_init();
    ensure_overlay_img();
}

bool weather_visuals_is_animated(int weather_code, bool is_day)
{
    (void)weather_code;
    (void)is_day;
    return false;
}

void weather_visuals_apply(lv_obj_t *img, uint8_t *frame_index, bool reset_frame, int weather_code, bool is_day)
{
    if (!img || !frame_index) {
        return;
    }

    if (weather_code < 0) {
        lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
        hide_overlay_img();
        *frame_index = 0;
        return;
    }

    lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
    hide_overlay_img();
    *frame_index = 0;
    (void)reset_frame;

    const WeatherVisualKind kind = weather_visual_kind_from_code(weather_code, is_day);
    const char *state_name = weather_sd_state_name(kind);
    const lv_img_dsc_t *sd_frame = nullptr;
    if (state_name) {
        const uint8_t frame_count = weather_sd_assets_frame_count(state_name);
        if (frame_count > 0) {
            if (weather_sd_assets_load_frame(state_name, 0, &sd_frame) && sd_frame) {
                apply_sd_frame(img, sd_frame, kSdPrimaryX, kSdPrimaryY, kSdPrimaryZoom);
                return;
            }
        }
    }

    switch (kind) {
    case WEATHER_VISUAL_CLEAR_DAY:
        apply_static_visual(img, lv_color_white(), LV_OPA_TRANSP, 86, -96, 1152);
        lv_img_set_src(img, kWeatherClearFrames[0]);
        break;
    case WEATHER_VISUAL_CLEAR_NIGHT:
        apply_sd_frame(img, kWeatherNightFrames[0], 82, -100, 288);
        break;
    case WEATHER_VISUAL_PARTLY_CLOUDY_DAY:
        apply_static_visual(img, lv_color_hex(0xB4C4D6), 217, 134, -72, 2112);
        break;
    case WEATHER_VISUAL_PARTLY_CLOUDY_NIGHT:
        apply_static_visual(img, lv_color_hex(0xB4C4D6), 217, 134, -72, 2112);
        break;
    case WEATHER_VISUAL_CLOUDY:
        apply_static_visual(img, lv_color_hex(0x89A6C5), 191, 138, -70, 2208);
        break;
    case WEATHER_VISUAL_FOG:
        apply_static_visual(img, lv_color_hex(0xB9C7D6), 200, 138, -70, 2208);
        break;
    case WEATHER_VISUAL_RAIN:
        apply_static_visual(img, lv_color_hex(0x4E86C9), 242, 140, -68, 2208);
        break;
    case WEATHER_VISUAL_STORM:
        apply_static_visual(img, lv_color_hex(0x7B67D8), LV_OPA_100, 144, -64, 2304);
        break;
    case WEATHER_VISUAL_SNOW:
        apply_static_visual(img, lv_color_hex(0xD8F1FF), LV_OPA_100, 136, -68, 2208);
        break;
    case WEATHER_VISUAL_STATIC:
    default:
        apply_static_visual(img, lv_color_white(), LV_OPA_TRANSP, 86, -96, 1152);
        break;
    }
}
