
#include "nfc_reader.hpp"
#include "audio_player.hpp"

#include <Arduino.h>
#include <Wire.h>


#include <SPI.h>
#include <SD.h>

#include <AudioFileSourceSD.h>
#include <AudioOutputI2S.h>

#include <AudioGeneratorWAV.h>

File myFile;
File dir;

AudioFileSourceSD* source = NULL;
AudioOutputI2S *out = NULL;
AudioGeneratorWAV *decoder = NULL;

#define SPI_SPEED SD_SCK_MHZ(40)


void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}




extern "C" void app_main()
{
  initArduino();

  // Arduino-like setup()
  Serial.begin(9600);
  while(!Serial){
    ; // wait for serial port to connect
  }


  AudioPlayer player;

  
  Serial.print("Initializing SD card...");
  SPIClass spi_sd(VSPI);

  if (!SD.begin(5, spi_sd)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  readFile(SD, "/test.txt");


  Adafruit_PN532& nfc = init_pn532();
  dir = SD.open("/"); 
  while(true){
    if ((decoder) && (decoder->isRunning())) {
      vTaskDelay(1);
      if (!decoder->loop()) decoder->stop();
    } else {
      File file = dir.openNextFile();
      if (file) {     
        if (String(file.name()).endsWith(".WAV")) {
          source->close();
          if (source->open(file.path())) {
            Serial.printf_P(PSTR("Playing '%s' from SD card...\n"), file.name());
            decoder->begin(source, out);
          } else {
            Serial.printf_P(PSTR("Error opening '%s'\n"), file.name());
          }
        } 
      } else {
        Serial.println(F("Playback from SD card done\n"));
        delay(1000);
      }       
    }
  }

  // WARNING: if program reaches end of function app_main() the MCU will restart.
}
