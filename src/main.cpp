
#include "nfc_reader.hpp"

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


bool play_next_song(File &dir, AudioGeneratorWAV *decoder, AudioFileSourceSD *source, AudioOutputI2S *out) {
  File file = dir.openNextFile();
  //Look for the next WAV file in the directory
  while (file && String(file.name()).endsWith(".WAV") == false) {
    file = dir.openNextFile();
  }
  if (!file) {
    Serial.println(F("Playback from SD card done\n"));
    return false;
  }
  
  source->close();
  if (source->open(file.path()) == false) {
    Serial.printf_P(PSTR("Error opening '%s'\n"), file.name());
    return false;
  }

  Serial.printf_P(PSTR("Playing '%s' from SD card...\n"), file.name());
  decoder->begin(source, out);
  
  return true;    
}

enum class State
{
  IDLE,
  MUSIC_ENDED,
  PLAYING
};


extern "C" void app_main()
{
  initArduino();

  // Arduino-like setup()
  Serial.begin(9600);
  while(!Serial){
    ; // wait for serial port to connect
  }


  source = new AudioFileSourceSD();
  out = new AudioOutputI2S(0, 1);
  decoder = new AudioGeneratorWAV();
  
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
  State state = State::IDLE;
  while(true){
    switch(state)
    {
      case State::PLAYING: {
        vTaskDelay(1);
        state = decoder->isRunning() ?  State::PLAYING : State::MUSIC_ENDED;
        if (!decoder->loop()) decoder->stop();
      }
      break;
      case State::MUSIC_ENDED:{
        const bool is_playing = play_next_song(dir, decoder, source, out);
        state = is_playing? State::PLAYING : State::IDLE;
      }
      break;
      case State::IDLE:{
        vTaskDelay(1000);
      }
      break;
    }
  }

  // WARNING: if program reaches end of function app_main() the MCU will restart.
}
