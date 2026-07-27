#include "wifi_board.h"
#include "display/lcd_display.h"
#include "codecs/es8389_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_touch_gt1151.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#define TAG "EspS31Korvo"


class EspS31Korvo : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Button boot_button_;
    AdcButton* btn_set_ = nullptr;
    AdcButton* btn_mode_ = nullptr;
    AdcButton* btn_vol_down_ = nullptr;
    AdcButton* btn_vol_up_ = nullptr;
    LcdDisplay* display_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    //初始化普通按键
    void InitializeButtons() {
        // NOTE: the schematic lists an ADC button on GPIO 42. The standard Button class is
        // digital; if the key does not react, the board needs an ADC-button driver instead.
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });
    }

    //初始化ADC按键
    void InitializeAdcButtons() {
    // ADC 按键阵列 (GPIO_NUM_42 -> ESP32-S31 ADC1_CH0)
    // 注意: ESP32-S31 的 ADC 引脚映射与 S3 不同, ADC1_CH0 = GPIO42, ADC1_CH1 = GPIO43。
    // 电路: 上拉 R77=10K, 各按键串联电阻接地 (1%精度)
    //   SET  (SW3) R79=13K  -> ~1870mV
    //   MODE (SW4) R80=6.8K  -> ~1340mV
    //   VOL- (SW5) R81=3.3K  -> ~820mV
    //   VOL+ (SW6) R82=1.3K  -> ~380mV

        // SET 按钮 (~1870mV 理论值): 切换 AEC 模式 (功能已与 MODE 互换)
        // 注意: ESP32-S31 的 ADC 无软件校准时走自定义公式 voltage = raw*4000/4393-2,
        // 会把读数整体放大约 13%, 1870mV 实测约 2111mV, 故上限必须放宽, 否则 SET 会被卡掉。
        // 该窗口在"校准开启(电压准确)"与"校准关闭(走公式)"两种情况下都安全, 且与 MODE(max=1550)不重叠。
        button_adc_config_t btn_set_config = {
            .adc_handle = NULL,
            .unit_id = ADC_UNIT_1,
            .adc_channel = 0,
            .button_index = 0,
            .min = 1650,
            .max = 2300,
        };
        btn_set_ = new AdcButton(btn_set_config);
        // SET 物理按键 -> 原 MODE 功能: 切换 AEC
        btn_set_->OnClick([this]() {
#if CONFIG_USE_DEVICE_AEC
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle ||
                app.GetDeviceState() == kDeviceStateListening) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
                ESP_LOGI(TAG, "AEC mode: %s", app.GetAecMode() == kAecOff ? "OFF" : "ON");
            }
#else
            ESP_LOGI(TAG, "SET pressed (AEC not enabled)");
#endif
        });

        // MODE 按钮 (~1340mV): 启动时配网 / 运行中切换聊天状态 (功能已与 SET 互换)
        button_adc_config_t btn_mode_config = {
        .adc_handle = NULL,
        .unit_id = ADC_UNIT_1,
        .adc_channel = 0,
        .button_index = 1,
            .min = 1150,
            .max = 1550,
        };
        btn_mode_ = new AdcButton(btn_mode_config);
        // MODE 物理按键 -> 原 SET 功能: 配网 / 切换聊天
        btn_mode_->OnClick([this]() {
            ESP_LOGI(TAG, "MODE button pressed");
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        // 音量辅助 lambda: 调节音量并显示提示
        auto adjust_volume = [this](int delta) {
            auto codec = Board::GetInstance().GetAudioCodec();
            if (!codec) return;
            int volume = codec->output_volume();
            volume = std::clamp(volume + delta, 0, 100);
            codec->SetOutputVolume(volume);
            ESP_LOGI(TAG, "Volume %s: %d", delta > 0 ? "up" : "down", volume);

        };

        // VOL- 按钮 (~820mV): 减小音量
        button_adc_config_t btn_vol_down_config = {
        .adc_handle = NULL,
        .unit_id = ADC_UNIT_1,
        .adc_channel = 0,
        .button_index = 2,
            .min = 650,
            .max = 1050,
        };
        btn_vol_down_ = new AdcButton(btn_vol_down_config);
        btn_vol_down_->OnClick([adjust_volume]() { adjust_volume(-15); });
        btn_vol_down_->OnLongPress([adjust_volume]() { adjust_volume(-30); });

        // VOL+ 按钮 (~380mV): 增大音量
        button_adc_config_t btn_vol_up_config = {
        .adc_handle = NULL,
        .unit_id = ADC_UNIT_1,
        .adc_channel = 0,
        .button_index = 3,
            .min = 250,
            .max = 550,
        };
        btn_vol_up_ = new AdcButton(btn_vol_up_config);
        btn_vol_up_->OnClick([adjust_volume]() { adjust_volume(15); });
        btn_vol_up_->OnLongPress([adjust_volume]() { adjust_volume(30); });
    }

    void InitializeRgbDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;   // RGB panels do not use a panel IO
        esp_lcd_panel_handle_t panel_handle = nullptr;

        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .timings = {
                .pclk_hz = 16 * 1000 * 1000,
                .h_res = DISPLAY_WIDTH,
                .v_res = DISPLAY_HEIGHT,
                .hsync_pulse_width = 4,
                .hsync_back_porch = 8,
                .hsync_front_porch = 8,
                .vsync_pulse_width = 4,
                .vsync_back_porch = 8,
                .vsync_front_porch = 8,
                .flags = {
                    .pclk_active_neg = true,
                },
            },
            .data_width = 16,
            .in_color_format = LCD_COLOR_FMT_RGB565,
            .out_color_format = LCD_COLOR_FMT_RGB565,
            .num_fbs = 2,
            // Bounce buffer 必须占用内部 DMA SRAM，驱动实测分配约为注释估算的 2 倍:
            //   40 行 -> 曾把内部 SRAM 压到只剩 8 字节
            //   8 行  -> 实测 ~51KB，idle 正常，但启动期(WiFi 连接+资源校验+模型加载)
            //            负载尖峰下仍触发 "LCD underrun" -> 横向错位/撕裂
            //   4 行  -> 实测 ~25KB，DMA 块太小必出 underrun，屏幕左右错位
            // 受内部 SRAM 限制(minimal sram 曾低至 ~2.6KB)，bounce 不能再加行，否则压穿 SRAM 崩溃。
            // refresh_on_demand 已验证会导致本封装初始化阻塞、屏幕不亮，故关闭。
            .bounce_buffer_size_px = DISPLAY_WIDTH * 8,
            .hsync_gpio_num = BSP_LCD_RGB_HSYNC,
            .vsync_gpio_num = BSP_LCD_RGB_VSYNC,
            .de_gpio_num = BSP_LCD_RGB_DE,
            .pclk_gpio_num = BSP_LCD_RGB_PCLK,
            .disp_gpio_num = BSP_LCD_RGB_DISP,
            .data_gpio_nums = {
                BSP_LCD_RGB_DATA0,  BSP_LCD_RGB_DATA1,  BSP_LCD_RGB_DATA2,  BSP_LCD_RGB_DATA3,
                BSP_LCD_RGB_DATA4,  BSP_LCD_RGB_DATA5,  BSP_LCD_RGB_DATA6,  BSP_LCD_RGB_DATA7,
                BSP_LCD_RGB_DATA8,  BSP_LCD_RGB_DATA9,  BSP_LCD_RGB_DATA10, BSP_LCD_RGB_DATA11,
                BSP_LCD_RGB_DATA12, BSP_LCD_RGB_DATA13, BSP_LCD_RGB_DATA14, BSP_LCD_RGB_DATA15,
            },
            .flags = {
                .fb_in_psram = 1,
            },
        };

        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&rgb_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        display_ = new RgbLcdDisplay(panel_io, panel_handle,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };

        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {};
        tp_io_config.scl_speed_hz = 400 * 1000;
        tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT1151_ADDRESS;
        tp_io_config.control_phase_bytes = 1;
        tp_io_config.dc_bit_offset = 0;
        tp_io_config.lcd_cmd_bits = 16;
        tp_io_config.flags.disable_control_phase = 1;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt1151(tp_io_handle, &tp_cfg, &tp));

        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "GT1151 touch initialized");
    }

public:
    EspS31Korvo() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        //InitializeButtons();
        esp_log_level_set("button_adc", ESP_LOG_DEBUG);
        InitializeAdcButtons();
        InitializeRgbDisplay();
        InitializeTouch();
        // Backlight pin is NC (hardwired on), so no backlight control is created.
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8389AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_SCLK,
            AUDIO_I2S_GPIO_LCLK,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            BSP_CODEC_ES8389_ADDR,
            AUDIO_CODEC_USE_MCLK); 
                    // use_mclk=false: 让 es8389_set_fs 执行 es8389_config_sample，
                    // 按 coeff 表配好时钟分频寄存器（与官方 atk_dnesp32s3_box2_wifi 一致）。
                     // 注意：I2S 硬件仍会在 MCLK 引脚输出 4.096MHz，此参数只影响 codec 内部分频配置。
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Led* GetLed() override {
        static SingleLed led(BSP_LED_RGB_IO);
        return &led;
    }
};

DECLARE_BOARD(EspS31Korvo);
