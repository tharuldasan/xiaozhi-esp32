# TAPPY ESP32-S3

## GPIO 48 RGB status LED

TAPPY uses one WS2812/NeoPixel data line on **GPIO 48**. The RGB color is encoded on that one wire; there are not separate R/G/B GPIO pins.

| State | Logical color | Effect |
|---|---|---|
| Wi-Fi / server connecting | `#00FF00` | Smooth green breathing |
| Wi-Fi lost | `#FF0000` | Solid red |
| Server/protocol connection lost | `#FFFF00` | Solid yellow |
| Ready | `#00FF00` | Solid green for 10 seconds, then dim green |
| Listening | `#0000FF` | Smooth blue breathing |
| TTS / speaking | `#FFFFFF` | Smooth white breathing |
| Processing | HSV rainbow | Fast RGB hue cycle at low brightness |
| Fatal error | `#FF0000` | Solid red |

The firmware keeps the single NeoPixel dim for eye comfort. The colors above are the logical/base colors; actual emitted channel values are brightness-scaled.

## Sound cues

The firmware reuses the existing compact built-in sounds:

- Listening/wake start: `popup.ogg`
- Stop listening / processing cue: `popup.ogg`
- Activation complete / ready: `success.ogg`

No additional binary sound assets are required.

## Build

The GitHub Actions workflow uses ESP-IDF 6.1 and builds `tappy-s3`. The firmware artifact includes `build/merged-binary.bin` plus the generated `.bin` files.
