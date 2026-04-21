#pragma once

#include <lvgl.h>

LV_FONT_DECLARE(font_huninn_14);
LV_FONT_DECLARE(font_huninn_18);
LV_FONT_DECLARE(font_huninn_24);
LV_FONT_DECLARE(font_huninn_32);
LV_FONT_DECLARE(font_huninn_48);

LV_FONT_DECLARE(font_mi_14);

// Material Symbols (Outlined) icon UTF-8 字串
// 搭配 font_mi_14 使用；codepoints 對照 main/fonts/icons.txt
#define MI_WIFI_0               "\xEF\x82\xB0"  // signal_wifi_0_bar
#define MI_WIFI_1               "\xEE\xAF\xA4"  // network_wifi_1_bar
#define MI_WIFI_2               "\xEE\xAF\x96"  // network_wifi_2_bar
#define MI_WIFI_3               "\xEE\xAF\xA1"  // network_wifi_3_bar
#define MI_WIFI_4               "\xEF\x81\xA5"  // signal_wifi_4_bar
#define MI_BLUETOOTH            "\xEE\x86\xA7"  // bluetooth
#define MI_BLUETOOTH_CONNECTED  "\xEE\x86\xA8"  // bluetooth_connected
#define MI_BLUETOOTH_DISABLED   "\xEE\x86\xA9"  // bluetooth_disabled
