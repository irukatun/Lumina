idf_component_register(
    SRCS "module_bus.c" "module_nvs.c" "module_boot.c" "main.c" # 需要編譯的 .c 檔案
    INCLUDE_DIRS "." # 讓 #include "xxx.h" 能找到 main/ 內的 .h
    REQUIRES # 依賴的 IDF 內建組件與第三方組件
    LDFRAGMENTS "esp_lcd_iram.lf" # 連結器規則
)