#include "wifi_board.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "codecs/no_audio_codec.h"
#include "display.h"

#define TAG "TappyS3"

class TappyS3 : public WifiBoard {
private:
    Button boot_button_;

public:
    TappyS3()
        : boot_button_(BOOT_BUTTON_GPIO) {

        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();

            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }

            app.ToggleChatState();
        });
    }

    Display* GetDisplay() override {
        // TAPPY uses esp32-eyes/U8g2 as the only OLED renderer.
        // Keeping LVGL away from the same SSD1306 prevents two display
        // drivers from fighting over the I2C bus.
        static NoDisplay display;
        return &display;
    }

    AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,

            // MAX98357A — speaker
            AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT,
            I2S_STD_SLOT_RIGHT,

            // INMP441 — microphone
            AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN,
            I2S_STD_SLOT_LEFT
        );

        return &audio_codec;
    }
};

DECLARE_BOARD(TappyS3);
