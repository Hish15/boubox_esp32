# boubox firmware

ESP32-S3 (ESP-IDF 5.5, C++) firmware of the boubox music box.

| Peripheral | Bus | Default pins (menuconfig → *Boubox Configuration*) |
|---|---|---|
| SD card | SPI2 | MOSI 11, MISO 13, SCLK 12, CS 10 |
| PN532 NFC | I2C0 (0x24) | SDA 8, SCL 9, RSTPDN not connected |
| I2S DAC/amp (e.g. MAX98357A, PCM5102) | I2S0 | BCLK 5, WS 6, DOUT 7 |
| Buttons (to GND) | GPIO | UP 15, DOWN 16, NEXT 17, PREVIOUS 18 |

## Behaviour
- Present an NTAG2xx tag: its content is the name of a folder at the SD card root. All `.wav` files
  of the folder (16-bit PCM, mono/stereo, any sample rate) are played in alphabetical order.
  The tag text is either an NDEF Text record (recommended) or a raw ASCII string.
  Names containing `/`, `\`, `:` or `..` are rejected.
- UP / DOWN: volume ±5 (auto-repeat when held). NEXT: next file (stops after the last one).
  PREVIOUS: restart the file if it played for more than 3 s, otherwise go to the previous file.
- Presenting another tag switches folder; a tag left on the reader is only handled once.

## Layout
- `components/pn532`: I2C PN532 driver + NDEF text parser
- `components/sd_card`: SPI SD card / FAT mount
- `components/audio_player`: WAV parser, I2S streaming task, volume, playlist
- `components/buttons`: debounced buttons with auto-repeat
- `main`: `Application` glue and `TagScanner` task

Build: `idf.py build flash monitor`
