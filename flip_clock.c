#include "flip_clock.h"

#include <string.h>

#define FLIP_DIGIT_COUNT 4

static const lv_coord_t kCardWidth = 100;
static const lv_coord_t kCardHeight = 144;
static const lv_coord_t kCardHalf = 72;
static const lv_coord_t kCardTopY = 122;
static const lv_coord_t kTopLabelY = -6;
static const lv_coord_t kCardXs[FLIP_DIGIT_COUNT] = {14, 126, 254, 366};

typedef struct
{
    lv_obj_t *card;
    lv_obj_t *top_half;
    lv_obj_t *bottom_half;
    lv_obj_t *top_wrapper;
    lv_obj_t *bottom_wrapper;
    lv_obj_t *top_label;
    lv_obj_t *bottom_label;
    lv_obj_t *ghost_layer;
    lv_obj_t *ghost_prev;
    lv_obj_t *old_top;
    lv_obj_t *old_top_wrapper;
    lv_obj_t *old_top_label;
    lv_obj_t *new_bottom;
    lv_obj_t *new_bottom_wrapper;
    lv_obj_t *new_bottom_label;
    char current_char;
    char next_char;
    bool animating;
} flip_digit_t;

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_week = NULL;
static lv_obj_t *s_date = NULL;
static lv_obj_t *s_holiday = NULL;
static lv_obj_t *s_timezone = NULL;
static lv_obj_t *s_colon_group = NULL;
static lv_obj_t *s_colon_top = NULL;
static lv_obj_t *s_colon_bottom = NULL;
static flip_digit_t s_digits[FLIP_DIGIT_COUNT];
static uint8_t s_last_hour = 255;
static uint8_t s_last_minute = 255;
static uint8_t s_last_second = 255;

static char sanitize_digit_char(char value)
{
    return (value >= '0' && value <= '9') ? value : '0';
}

static void set_label_char(lv_obj_t *label, char value)
{
    const char safe = sanitize_digit_char(value);
    char text[2] = {safe, 0};
    lv_label_set_text(label, text);
}

static void build_time_digits(uint8_t hour24, uint8_t minute, char out_digits[FLIP_DIGIT_COUNT])
{
    out_digits[0] = (char)('0' + ((hour24 / 10U) % 10U));
    out_digits[1] = (char)('0' + (hour24 % 10U));
    out_digits[2] = (char)('0' + ((minute / 10U) % 10U));
    out_digits[3] = (char)('0' + (minute % 10U));
}

static void build_prev_minute_digits(uint8_t hour24, uint8_t minute, char out_digits[FLIP_DIGIT_COUNT])
{
    uint16_t total = (uint16_t)hour24 * 60U + (uint16_t)minute;
    total = (uint16_t)((total + (24U * 60U) - 1U) % (24U * 60U));
    const uint8_t prev_hour = (uint8_t)(total / 60U);
    const uint8_t prev_min = (uint8_t)(total % 60U);
    build_time_digits(prev_hour, prev_min, out_digits);
}
static void style_half_panel(lv_obj_t *obj, bool bottom)
{
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bottom ? 0x0b1117 : 0x151d27), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x2d3947), 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void style_overlay(lv_obj_t *obj, bool bottom)
{
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bottom ? 0x0f1720 : 0x18212d), 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_digit_label(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &ui_font_Font1, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xf2f6fb), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(label, kCardWidth, kCardHalf);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -2);
    lv_label_set_text(label, "0");
    return label;
}

static lv_obj_t *create_prev_digit_label(lv_obj_t *parent, lv_coord_t x_ofs, lv_coord_t y_ofs, lv_opa_t opa, uint16_t zoom)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &ui_font_Font1, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xc9daea), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(label, opa, 0);
    lv_obj_set_size(label, kCardWidth, kCardHalf);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, x_ofs, y_ofs);
    lv_obj_set_style_transform_pivot_x(label, kCardWidth / 2, 0);
    lv_obj_set_style_transform_pivot_y(label, kCardHalf / 2, 0);
    lv_obj_set_style_transform_angle(label, 0, 0);
    lv_obj_set_style_transform_zoom(label, zoom, 0);
    lv_label_set_text(label, "0");
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    return label;
}

static void set_digit_prev_hint(flip_digit_t *digit, char value, bool visible)
{
    if (!digit)
    {
        return;
    }

    if (!digit->ghost_prev)
    {
        return;
    }

    if (!visible)
    {
        lv_obj_add_flag(digit->ghost_prev, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const char safe = sanitize_digit_char(value);
    char text[2] = {safe, 0};
    lv_label_set_text(digit->ghost_prev, text);
    lv_obj_clear_flag(digit->ghost_prev, LV_OBJ_FLAG_HIDDEN);
}

static void build_digit(flip_digit_t *digit, lv_obj_t *parent, lv_coord_t x)
{
    memset(digit, 0, sizeof(*digit));

    digit->card = lv_obj_create(parent);
    lv_obj_set_size(digit->card, kCardWidth, kCardHeight);
    lv_obj_set_pos(digit->card, x, kCardTopY);
    lv_obj_set_style_bg_opa(digit->card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(digit->card, 0, 0);
    lv_obj_set_style_pad_all(digit->card, 0, 0);
    lv_obj_clear_flag(digit->card, LV_OBJ_FLAG_SCROLLABLE);

    digit->top_half = lv_obj_create(digit->card);
    lv_obj_set_pos(digit->top_half, 0, 0);
    lv_obj_set_size(digit->top_half, kCardWidth, kCardHalf);
    style_half_panel(digit->top_half, false);

    digit->top_wrapper = lv_obj_create(digit->top_half);
    lv_obj_set_pos(digit->top_wrapper, 0, 0);
    lv_obj_set_size(digit->top_wrapper, kCardWidth, kCardHeight);
    lv_obj_set_style_bg_opa(digit->top_wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(digit->top_wrapper, 0, 0);
    lv_obj_set_style_pad_all(digit->top_wrapper, 0, 0);
    lv_obj_clear_flag(digit->top_wrapper, LV_OBJ_FLAG_SCROLLABLE);
    digit->top_label = create_digit_label(digit->top_wrapper);

    digit->bottom_half = lv_obj_create(digit->card);
    lv_obj_set_pos(digit->bottom_half, 0, kCardHalf);
    lv_obj_set_size(digit->bottom_half, kCardWidth, kCardHalf);
    style_half_panel(digit->bottom_half, true);

    digit->ghost_layer = lv_obj_create(digit->card);
    lv_obj_set_pos(digit->ghost_layer, 0, kCardHalf);
    lv_obj_set_size(digit->ghost_layer, kCardWidth, kCardHalf);
    lv_obj_set_style_bg_opa(digit->ghost_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(digit->ghost_layer, 0, 0);
    lv_obj_set_style_pad_all(digit->ghost_layer, 0, 0);
    lv_obj_clear_flag(digit->ghost_layer, LV_OBJ_FLAG_SCROLLABLE);

    digit->ghost_prev = create_prev_digit_label(digit->ghost_layer, 0, 0, 96, 190);

    digit->bottom_wrapper = lv_obj_create(digit->bottom_half);
    lv_obj_set_pos(digit->bottom_wrapper, 0, -kCardHalf);
    lv_obj_set_size(digit->bottom_wrapper, kCardWidth, kCardHeight);
    lv_obj_set_style_bg_opa(digit->bottom_wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(digit->bottom_wrapper, 0, 0);
    lv_obj_set_style_pad_all(digit->bottom_wrapper, 0, 0);
    lv_obj_clear_flag(digit->bottom_wrapper, LV_OBJ_FLAG_SCROLLABLE);
    digit->bottom_label = create_digit_label(digit->bottom_wrapper);

    lv_obj_t *divider = lv_obj_create(digit->card);
    lv_obj_set_size(divider, kCardWidth, 4);
    lv_obj_set_pos(divider, 0, kCardHalf - 2);
    lv_obj_set_style_radius(divider, 0, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x344456), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    digit->old_top = lv_obj_create(digit->card);
    lv_obj_set_pos(digit->old_top, 0, 0);
    lv_obj_set_size(digit->old_top, kCardWidth, kCardHalf);
    style_overlay(digit->old_top, false);
    lv_obj_add_flag(digit->old_top, LV_OBJ_FLAG_HIDDEN);

    digit->old_top_wrapper = lv_obj_create(digit->old_top);
    lv_obj_set_pos(digit->old_top_wrapper, 0, 0);
    lv_obj_set_size(digit->old_top_wrapper, kCardWidth, kCardHeight);
    lv_obj_set_style_bg_opa(digit->old_top_wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(digit->old_top_wrapper, 0, 0);
    lv_obj_set_style_pad_all(digit->old_top_wrapper, 0, 0);
    lv_obj_clear_flag(digit->old_top_wrapper, LV_OBJ_FLAG_SCROLLABLE);
    digit->old_top_label = create_digit_label(digit->old_top_wrapper);

    digit->new_bottom = lv_obj_create(digit->card);
    lv_obj_set_pos(digit->new_bottom, 0, kCardHalf);
    lv_obj_set_size(digit->new_bottom, kCardWidth, 0);
    style_overlay(digit->new_bottom, true);
    lv_obj_add_flag(digit->new_bottom, LV_OBJ_FLAG_HIDDEN);

    digit->new_bottom_wrapper = lv_obj_create(digit->new_bottom);
    lv_obj_set_pos(digit->new_bottom_wrapper, 0, -kCardHalf);
    lv_obj_set_size(digit->new_bottom_wrapper, kCardWidth, kCardHeight);
    lv_obj_set_style_bg_opa(digit->new_bottom_wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(digit->new_bottom_wrapper, 0, 0);
    lv_obj_set_style_pad_all(digit->new_bottom_wrapper, 0, 0);
    lv_obj_clear_flag(digit->new_bottom_wrapper, LV_OBJ_FLAG_SCROLLABLE);
    digit->new_bottom_label = create_digit_label(digit->new_bottom_wrapper);

    digit->current_char = '0';
    digit->next_char = '0';
    set_digit_prev_hint(digit, '0', true);
}

static void apply_digit_now(flip_digit_t *digit, char value)
{
    set_label_char(digit->top_label, value);
    set_label_char(digit->bottom_label, value);
    set_label_char(digit->old_top_label, value);
    set_label_char(digit->new_bottom_label, value);
    digit->current_char = value;
    digit->next_char = value;
    digit->animating = false;
    lv_obj_add_flag(digit->old_top, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(digit->new_bottom, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(digit->old_top, kCardHalf);
    lv_obj_set_height(digit->new_bottom, 0);
}

static void flip_digit_phase2_exec(void *var, int32_t value)
{
    flip_digit_t *digit = (flip_digit_t *)var;
    lv_obj_set_height(digit->new_bottom, value);
}

static void flip_digit_phase2_ready(lv_anim_t *anim)
{
    flip_digit_t *digit = (flip_digit_t *)anim->var;
    set_label_char(digit->bottom_label, digit->next_char);
    lv_obj_add_flag(digit->new_bottom, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(digit->new_bottom, 0);
    digit->current_char = digit->next_char;
    digit->animating = false;
}

static void start_phase2(flip_digit_t *digit)
{
    set_label_char(digit->top_label, digit->next_char);
    set_label_char(digit->new_bottom_label, digit->next_char);
    lv_obj_clear_flag(digit->new_bottom, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(digit->new_bottom, 0);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, digit);
    lv_anim_set_values(&anim, 0, kCardHalf);
    lv_anim_set_time(&anim, 120);
    lv_anim_set_exec_cb(&anim, flip_digit_phase2_exec);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&anim, flip_digit_phase2_ready);
    lv_anim_start(&anim);
}

static void flip_digit_phase1_exec(void *var, int32_t value)
{
    flip_digit_t *digit = (flip_digit_t *)var;
    lv_obj_set_height(digit->old_top, value);
}

static void flip_digit_phase1_ready(lv_anim_t *anim)
{
    flip_digit_t *digit = (flip_digit_t *)anim->var;
    lv_obj_add_flag(digit->old_top, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(digit->old_top, kCardHalf);
    start_phase2(digit);
}

static bool animate_digit_to(flip_digit_t *digit, char value)
{
    if (digit->current_char == value && !digit->animating)
    {
        return false;
    }

    if (digit->animating)
    {
        apply_digit_now(digit, value);
        return true;
    }

    digit->next_char = value;
    digit->animating = true;
    set_label_char(digit->old_top_label, digit->current_char);
    lv_obj_clear_flag(digit->old_top, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(digit->old_top, kCardHalf);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, digit);
    lv_anim_set_values(&anim, kCardHalf, 0);
    lv_anim_set_time(&anim, 110);
    lv_anim_set_exec_cb(&anim, flip_digit_phase1_exec);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&anim, flip_digit_phase1_ready);
    lv_anim_start(&anim);
    return true;
}

static void create_colon(lv_obj_t *parent)
{
    s_colon_group = lv_obj_create(parent);
    lv_obj_set_size(s_colon_group, 12, 62);
    lv_obj_set_pos(s_colon_group, 234, 166);
    lv_obj_set_style_bg_opa(s_colon_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_colon_group, 0, 0);
    lv_obj_set_style_pad_all(s_colon_group, 0, 0);
    lv_obj_clear_flag(s_colon_group, LV_OBJ_FLAG_SCROLLABLE);

    s_colon_top = lv_obj_create(s_colon_group);
    lv_obj_set_size(s_colon_top, 12, 12);
    lv_obj_set_pos(s_colon_top, 0, 0);
    lv_obj_set_style_radius(s_colon_top, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_colon_top, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_colon_top, lv_color_hex(0x7fd0ff), 0);
    lv_obj_set_style_border_width(s_colon_top, 0, 0);
    lv_obj_clear_flag(s_colon_top, LV_OBJ_FLAG_SCROLLABLE);

    s_colon_bottom = lv_obj_create(s_colon_group);
    lv_obj_set_size(s_colon_bottom, 12, 12);
    lv_obj_set_pos(s_colon_bottom, 0, 50);
    lv_obj_set_style_radius(s_colon_bottom, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_colon_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_colon_bottom, lv_color_hex(0x7fd0ff), 0);
    lv_obj_set_style_border_width(s_colon_bottom, 0, 0);
    lv_obj_clear_flag(s_colon_bottom, LV_OBJ_FLAG_SCROLLABLE);
}

void flip_clock_create(lv_obj_t *parent)
{
    s_root = parent;
    s_last_hour = 255;
    s_last_minute = 255;
    s_last_second = 255;

    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x061019), 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x12293f), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(parent, 0, 0);

    s_title = lv_label_create(parent);
    lv_label_set_text(s_title, "FLIP CLOCK");
    lv_obj_set_style_text_font(s_title, &ui_font_Font4, 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0x6ea4c9), 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 24);

    s_week = lv_label_create(parent);
    lv_label_set_text(s_week, "---");
    lv_obj_set_style_text_font(s_week, &ui_font_Font4, 0);
    lv_obj_set_style_text_color(s_week, lv_color_hex(0xf2f6fb), 0);
    lv_obj_align(s_week, LV_ALIGN_TOP_MID, 0, 52);

    s_date = lv_label_create(parent);
    lv_label_set_text(s_date, "----/--/--");
    lv_obj_set_style_text_font(s_date, &ui_font_Font2, 0);
    lv_obj_set_style_text_color(s_date, lv_color_hex(0x97a9bb), 0);
    lv_obj_align(s_date, LV_ALIGN_TOP_MID, 0, 82);

    create_colon(parent);

    for (int i = 0; i < FLIP_DIGIT_COUNT; ++i)
    {
        build_digit(&s_digits[i], parent, kCardXs[i]);
        apply_digit_now(&s_digits[i], '0');
    }

    s_holiday = lv_label_create(parent);
    lv_label_set_text(s_holiday, "");
    lv_obj_set_style_text_font(s_holiday, &ui_font_Font4, 0);
    lv_obj_set_style_text_color(s_holiday, lv_color_hex(0xcad8e6), 0);
    lv_obj_set_style_text_align(s_holiday, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_holiday, 430);
    lv_label_set_long_mode(s_holiday, LV_LABEL_LONG_DOT);
    lv_obj_align(s_holiday, LV_ALIGN_BOTTOM_MID, 0, -62);

    s_timezone = lv_label_create(parent);
    lv_label_set_text(s_timezone, "Kyiv time - EET");
    lv_obj_set_style_text_font(s_timezone, &ui_font_Font4, 0);
    lv_obj_set_style_text_color(s_timezone, lv_color_hex(0x7a90a6), 0);
    lv_obj_align(s_timezone, LV_ALIGN_BOTTOM_MID, 0, -36);
}

bool flip_clock_set_datetime(uint8_t hour24, uint8_t minute, uint8_t second, const char *week_text, const char *date_text, const char *holiday_text, const char *timezone_text)
{
    if (!s_root)
    {
        return false;
    }

    if (week_text && s_week)
    {
        lv_label_set_text(s_week, week_text);
    }
    if (date_text && s_date)
    {
        lv_label_set_text(s_date, date_text);
    }
    if (holiday_text && s_holiday)
    {
        lv_label_set_text(s_holiday, holiday_text);
    }
    if (timezone_text && s_timezone)
    {
        lv_label_set_text(s_timezone, timezone_text);
    }

    char digits[FLIP_DIGIT_COUNT] = {0};
    char prev_digits[FLIP_DIGIT_COUNT] = {0};
    build_time_digits(hour24, minute, digits);
    build_prev_minute_digits(hour24, minute, prev_digits);

    bool changed = false;
    if (s_last_hour == 255)
    {
        for (int i = 0; i < FLIP_DIGIT_COUNT; ++i)
        {
            apply_digit_now(&s_digits[i], digits[i]);
            set_digit_prev_hint(&s_digits[i], prev_digits[i], true);
        }
    }
    else
    {
        for (int i = 0; i < FLIP_DIGIT_COUNT; ++i)
        {
            set_digit_prev_hint(&s_digits[i], prev_digits[i], true);
            if (animate_digit_to(&s_digits[i], digits[i]))
            {
                changed = true;
            }
        }
    }

    const bool colon_on = ((second & 1U) == 0U);
    if (s_colon_group)
    {
        if (colon_on)
        {
            lv_obj_clear_flag(s_colon_group, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(s_colon_group, LV_OBJ_FLAG_HIDDEN);
        }
    }

    s_last_hour = hour24;
    s_last_minute = minute;
    s_last_second = second;
    return changed;
}












