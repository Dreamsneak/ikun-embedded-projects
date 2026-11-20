idf_component_register(SRCS "main.c" "lcd_ili9341.c" "battery_adc.c" "mlx90640.c" "MLX90640_API.c" "thermal_display.c" "sd_card.c"
                    INCLUDE_DIRS ""
                    REQUIRES driver spi_flash fatfs)
