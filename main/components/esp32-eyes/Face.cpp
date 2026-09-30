#include "Face.h"
#include "Common.h"
#include <esp_timer.h>
#include <driver/i2c.h>

static unsigned long now_now_millis() {
  return static_cast<unsigned long>(esp_timer_get_time() / 1000ULL);
}

u8g2_t u8g2;

Face::Face(uint16_t screenWidth, uint16_t screenHeight, uint16_t eyeSize)
    : LeftEye(*this), RightEye(*this), Blink(*this), Look(*this), Behavior(*this), Expression(*this) {

  static bool display_initialized = false;
  if (!display_initialized) {
    u8g2_esp32_i2c_ctx_t ctx = {
      .i2c_port = I2C_NUM_0,
      .sda_pin = GPIO_NUM_15,
      .scl_pin = GPIO_NUM_7,
      .dev_addr_7bit = 0x3C,
      .clk_hz = 400000,
      .timeout_ms = 1000,
      .reset_pin = U8G2_ESP32_PIN_UNUSED
    };
    u8g2_esp32_i2c_set_default_context(&ctx);
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
      &u8g2,
      U8G2_R0,
      u8x8_byte_esp32_hw_i2c,
      u8x8_gpio_and_delay_esp32_i2c
    );
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x3C << 1);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    display_initialized = true;
  }

  u8g2_ClearBuffer(&u8g2);

  Width = screenWidth;
  Height = screenHeight;
  EyeSize = eyeSize;

  CenterX = Width / 2;
  CenterY = Height / 2;

  LeftEye.IsMirrored = true;

  Behavior.Clear();
  Behavior.Timer.Start();
}

void Face::LookFront() {
  Look.LookAt(0.0, 0.0);
}

void Face::LookRight() {
  Look.LookAt(-1.0, 0.0);
}

void Face::LookLeft() {
  Look.LookAt(1.0, 0.0);
}

void Face::LookTop() {
  Look.LookAt(0.0, 1.0);
}

void Face::LookBottom() {
  Look.LookAt(0.0, -1.0);
}

void Face::Wait(unsigned long milliseconds) {
  unsigned long start = millis();
  while (millis() - start < milliseconds) {
    Draw();
  }
}

void Face::DoBlink() {
  Blink.Blink();
}

void Face::Update() {
  if (RandomBehavior) Behavior.Update();
  if (RandomLook) Look.Update();
  if (RandomBlink) Blink.Update();
  Draw();
}

void Face::Draw() {
  u8g2_ClearBuffer(&u8g2);

  LeftEye.CenterX = CenterX - EyeSize / 2 - EyeInterDistance;
  LeftEye.CenterY = CenterY;
  LeftEye.Draw();

  RightEye.CenterX = CenterX + EyeSize / 2 + EyeInterDistance;
  RightEye.CenterY = CenterY;
  RightEye.Draw();

  u8g2_SendBuffer(&u8g2);
}
