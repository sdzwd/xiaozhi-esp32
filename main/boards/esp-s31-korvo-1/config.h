#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

/**************************************************************************************************
 *  Esp32-S31-korvo-1 board pinout (ESP32-S31)
 **************************************************************************************************/

/* I2C (codec + touch share the same bus) */
#define BSP_I2C_SCL  GPIO_NUM_1
#define BSP_I2C_SDA  GPIO_NUM_0

/* Audio - ES8389 codec */
#define AUDIO_INPUT_SAMPLE_RATE   16000
#define AUDIO_OUTPUT_SAMPLE_RATE  16000

#define AUDIO_I2S_GPIO_MCLK  GPIO_NUM_2   
#define AUDIO_I2S_GPIO_SCLK  GPIO_NUM_3
#define AUDIO_I2S_GPIO_LCLK  GPIO_NUM_4
#define AUDIO_I2S_GPIO_DOUT  GPIO_NUM_5    // to codec DAC
#define AUDIO_I2S_GPIO_DIN   GPIO_NUM_6    // from codec ADC
#define AUDIO_CODEC_PA_PIN   GPIO_NUM_7    // power amplifier enable

#define AUDIO_CODEC_I2C_SDA_PIN  BSP_I2C_SDA
#define AUDIO_CODEC_I2C_SCL_PIN  BSP_I2C_SCL
// ES8389 I2C address: audio_codec expects the 8-bit address (incl. R/W bit).
// 7-bit slave addr is 0x10 (AD0=0) / 0x11 (AD0=1); 8-bit write addr is 0x20 / 0x22.
#define BSP_CODEC_ES8389_ADDR  0x20   // AD0=0; change to 0x22 if AD0 is pulled high
#define AUDIO_CODEC_ES8389_ADDR ES8389_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_USE_MCLK false

/* Display - RGB 16-bit, ST7262E43 controller.
 * Physical panel is 800x480 (landscape). To use it in portrait we swap the
 * logical resolution to 480x800 and let the panel controller rotate the scan
 * via DISPLAY_SWAP_XY. (Mirrors stay false; flip DISPLAY_MIRROR_X/Y if the
 * portrait image comes out upside-down or mirrored.) */
#define DISPLAY_WIDTH   800
#define DISPLAY_HEIGHT  480
#define DISPLAY_MIRROR_X  false
#define DISPLAY_MIRROR_Y  false
#define DISPLAY_SWAP_XY   false
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define BSP_LCD_RGB_VSYNC  GPIO_NUM_45
#define BSP_LCD_RGB_HSYNC  GPIO_NUM_44
#define BSP_LCD_RGB_DE     GPIO_NUM_43
#define BSP_LCD_RGB_PCLK   GPIO_NUM_40
#define BSP_LCD_RGB_DISP   GPIO_NUM_38
#define BSP_LCD_RGB_DATA0  GPIO_NUM_8
#define BSP_LCD_RGB_DATA1  GPIO_NUM_9
#define BSP_LCD_RGB_DATA2  GPIO_NUM_10
#define BSP_LCD_RGB_DATA3  GPIO_NUM_11
#define BSP_LCD_RGB_DATA4  GPIO_NUM_12
#define BSP_LCD_RGB_DATA5  GPIO_NUM_13
#define BSP_LCD_RGB_DATA6  GPIO_NUM_14
#define BSP_LCD_RGB_DATA7  GPIO_NUM_15
#define BSP_LCD_RGB_DATA8  GPIO_NUM_16
#define BSP_LCD_RGB_DATA9  GPIO_NUM_17
#define BSP_LCD_RGB_DATA10 GPIO_NUM_18
#define BSP_LCD_RGB_DATA11 GPIO_NUM_19
#define BSP_LCD_RGB_DATA12 GPIO_NUM_33
#define BSP_LCD_RGB_DATA13 GPIO_NUM_34
#define BSP_LCD_RGB_DATA14 GPIO_NUM_35
#define BSP_LCD_RGB_DATA15 GPIO_NUM_36
#define BSP_LCD_RST        GPIO_NUM_NC
#define BSP_LCD_BACKLIGHT  GPIO_NUM_NC   // backlight assumed hardwired-on; no SW control
#define BSP_LCD_TOUCH_INT  GPIO_NUM_NC

/* Touch - GT1151 over I2C (address from esp_lcd_touch_gt1151.h) */
#define BSP_TOUCH_RST      GPIO_NUM_NC

/* LED - RGB status LED on a single GPIO (WS2812-style) */
#define BSP_LED_RGB_IO  GPIO_NUM_37

/* Buttons - Boot button on GPIO 61 (see note in board .cc) */
#define BOOT_BUTTON_GPIO  GPIO_NUM_61

// ===== ADC 按键 (GPIO_NUM_42) =====
#define ADC_BUTTON_GPIO GPIO_NUM_42

/* USB */
#define BSP_USB_POS  GPIO_NUM_20
#define BSP_USB_NEG  GPIO_NUM_19

/* Camera (parallel DVP) - pins defined for reference; camera support on ESP32-S31
   requires the esp_video / parallel-camera path and is not wired in this initial board. */
#define BSP_CAMERA_GPIO_XCLK GPIO_NUM_55
#define BSP_CAMERA_RST       GPIO_NUM_NC
#define BSP_CAMERA_PCLK      GPIO_NUM_54
#define BSP_CAMERA_VSYNC     GPIO_NUM_56
#define BSP_CAMERA_HSYNC     GPIO_NUM_57
#define BSP_CAMERA_D0        GPIO_NUM_46
#define BSP_CAMERA_D1        GPIO_NUM_47
#define BSP_CAMERA_D2        GPIO_NUM_48
#define BSP_CAMERA_D3        GPIO_NUM_49
#define BSP_CAMERA_D4        GPIO_NUM_50
#define BSP_CAMERA_D5        GPIO_NUM_51
#define BSP_CAMERA_D6        GPIO_NUM_52
#define BSP_CAMERA_D7        GPIO_NUM_53

/* SD card */
#define BSP_SD_D0       GPIO_NUM_20
#define BSP_SD_D1       GPIO_NUM_21
#define BSP_SD_D2       GPIO_NUM_22
#define BSP_SD_D3       GPIO_NUM_23
#define BSP_SD_CMD      GPIO_NUM_25
#define BSP_SD_CLK      GPIO_NUM_24
#define BSP_SD_DET      GPIO_NUM_NC
#define BSP_SD_SPI_CLK  GPIO_NUM_24
#define BSP_SD_SPI_MISO GPIO_NUM_20
#define BSP_SD_SPI_MOSI GPIO_NUM_25
#define BSP_SD_SPI_CS   GPIO_NUM_23
#define BSP_SD_EN       GPIO_NUM_39

#endif // _BOARD_CONFIG_H_
