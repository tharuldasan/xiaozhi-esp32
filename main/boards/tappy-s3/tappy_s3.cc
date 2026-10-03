#include "wifi_board.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "codecs/no_audio_codec.h"
#include "display.h"
#include "alarm/tappy_alarm_manager.h"
#include "led/single_led.h"

#define TAG "TappyS3"

class TappyS3 : public WifiBoard {
private:
    Button boot_button_;

public:
    TappyS3()
        : boot_button_(BOOT_BUTTON_GPIO) {

        boot_button_.OnClick([this]() {
            auto& alarm = TappyAlarmManager::GetInstance();
            if (alarm.IsRinging()) {
                alarm.DismissRinging();
                return;
            }
            auto& app = Application::GetInstance();

            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }

            app.ToggleChatState();
        });
    }

    Led* GetLed() override {
        static SingleLed led(GPIO_NUM_48);
        return &led;
    }

    Display* GetDisplay() override {
        // TAPPY uses esp32-eyes/U8g2 as the OLED renderer.
        // Keep the normal Xiaozhi LVGL OLED path disabled for this board;
        // otherwise it would create a second SSD1306/I2C stack.
        static NoDisplay display;
        return &display;
    }

    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT,
            I2S_STD_SLOT_LEFT,
            AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN,
            I2S_STD_SLOT_LEFT
        );

        return &audio_codec;
    }
};

DECLARE_BOARD(TappyS3);
