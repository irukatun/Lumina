idf_component_register(
    SRCS # 需要編譯的 .c 檔案
        module_ui_boot.c
        module_display.c
        module_touch.c
        module_lvgl.c
        module_sd.c
        module_bus.c
        module_nvs.c
        module_boot.c
        main.c
    INCLUDE_DIRS "." # 讓 #include "xxx.h" 能找到 main/ 內的 .h
    REQUIRES # 依賴的 IDF 內建組件與第三方組件
        driver
        nvs_flash
        fatfs
        sdmmc
        esp_lcd
        esp_system
        atanisoft__esp_lcd_ili9488
        espressif__esp_lcd_touch
        atanisoft__esp_lcd_touch_xpt2046
        espressif__esp_lvgl_port
        lvgl__lvgl
    LDFRAGMENTS "esp_lcd_iram.lf" # 連結器規則
)