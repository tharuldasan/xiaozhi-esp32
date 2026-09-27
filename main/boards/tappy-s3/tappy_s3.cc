#include "wifi_board.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "codecs/no_audio_codec.h"

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

    AudioCodec* GetAudioCodec() override {

        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,

            // MAX98357A
            AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT,

            // INMP441
            AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN
        );

        return &audio_codec;
    }
};

DECLARE_BOARD(TappyS3);
