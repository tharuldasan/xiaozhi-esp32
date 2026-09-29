#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ============================================================
// AUDIO
// ============================================================

// INMP441 input
#define AUDIO_INPUT_SAMPLE_RATE 16000

// MAX98357A output
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// We have separate I2S buses for microphone and speaker.
#define AUDIO_I2S_METHOD_SIMPLEX

// ------------------------------------------------------------
// INMP441
// ------------------------------------------------------------

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6

// ------------------------------------------------------------
// MAX98357A
// ------------------------------------------------------------

#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_42
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_45
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_47

// ------------------------------------------------------------
// BOOT BUTTON
// ------------------------------------------------------------

#define BOOT_BUTTON_GPIO GPIO_NUM_0

// No volume buttons for now.
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

// TAPPY WS2812/NeoPixel status LED: single RGB data line on GPIO 48.
#define BUILTIN_LED_GPIO GPIO_NUM_48

// ============================================================
// SSD1306
// ============================================================

#define DISPLAY_SDA_PIN GPIO_NUM_15
#define DISPLAY_SCL_PIN GPIO_NUM_7

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64

#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false

#endif
