idf_component_register(
    SRCS # 需要編譯的 .c 檔案
        fonts/font_huninn_14.c
        fonts/font_huninn_18.c
        fonts/font_huninn_24.c
        fonts/font_huninn_32.c
        fonts/font_huninn_48.c
        module_pir.c
        module_imu.c
        module_time.c
        module_ui_boot.c
        module_display.c
        module_touch.c
        module_lvgl.c
        module_sd.c
        module_bus.c
        module_nvs.c
        module_boot.c
        main.c
    INCLUDE_DIRS "." "fonts" # 讓 #include "xxx.h" 能找到 main/ 與 main/fonts/ 內的 .h
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
        k0i05__esp_mpu6050
    LDFRAGMENTS "esp_lcd_iram.lf" # 連結器規則
)