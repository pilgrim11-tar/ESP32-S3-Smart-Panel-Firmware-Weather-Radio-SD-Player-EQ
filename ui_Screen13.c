#include "ui.h"
#include "flip_clock.h"

void ui_Screen13_screen_init(void)
{
    ui_Screen13 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen13, LV_OBJ_FLAG_SCROLLABLE);

    ui_Image52 = NULL;
    ui_Image53 = NULL;
    ui_Image54 = NULL;
    ui_Image55 = NULL;
    ui_Image56 = NULL;
    ui_Image57 = NULL;
    ui_Image58 = NULL;
    ui_Image59 = NULL;
    ui_Image60 = NULL;
    ui_Image61 = NULL;
    ui_Image62 = NULL;

    flip_clock_create(ui_Screen13);

    lv_obj_add_event_cb(ui_Screen13, ui_event_Screen13, LV_EVENT_ALL, NULL);
}
