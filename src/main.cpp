
#include "nfc_reader.hpp"

#include <Arduino.h>
#include <Wire.h>


#include <SPI.h>
#include <SD.h>

#include <AudioFileSourceSD.h>
#include <AudioOutputI2S.h>

#include <AudioGeneratorWAV.h>

#include <thread>
#include <chrono>
#include <memory>

File myFile;
File dir;



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

class AudioPlayer {
  public:
  AudioFileSourceSD source;
  AudioOutputI2S out;
  AudioGeneratorWAV decoder;
  std::thread player_thread;
  std::array<char,256> current_dir;
  bool keep_playing = false;
  AudioPlayer() : source(), out(0, 1), decoder() {}
  ~AudioPlayer() {
    Serial.println(F("AudioPlayer destructor called"));
    keep_playing = false;
    if (player_thread.joinable())
    {
      player_thread.join();
    }
  }

  void play_folder_assync(const char* path)
  {
    std::copy_if(path, path + current_dir.size(), current_dir.begin(), [](char c) { return c != '\0'; });
    auto task = [](void* obj) {
      static_cast<AudioPlayer*>(obj)->play_folder();
      vTaskDelete(NULL);
    };
    xTaskCreate(task, "AudioPlayerTask", 4096, this, 1, NULL);
  }


  void play_folder()
  {
    Serial.printf_P(PSTR("Playing folder '%s' from SD card...\n"), current_dir.data());
    auto dir = SD.open(current_dir.data());
    keep_playing = true;
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1ms);
    while(keep_playing){
      
      if (decoder.isRunning())
      {
        std::this_thread::sleep_for(1ms);
        if (!decoder.loop())
        {
          decoder.stop();
        }
        continue;
      }
      File file = dir.openNextFile();
      if (file == false)
      {
        Serial.println(F("Playback from SD card done\n"));
        std::this_thread::sleep_for(2000ms);
        break;
      }
      if (String(file.name()).endsWith(".WAV")) {
        source.close();
        if (source.open(file.path())) {
          Serial.printf_P(PSTR("Playing '%s' from SD card...\n"), file.name());
          decoder.begin(&source, &out);
        } else {
          Serial.printf_P(PSTR("Error opening '%s'\n"), file.name());
        }
      } 
      
      
    }
  }


};


extern "C" void app_main()
{
  initArduino();

  // Arduino-like setup()
  Serial.begin(9600);
  while(!Serial){
    ; // wait for serial port to connect
  }


  
  AudioPlayer* player = new AudioPlayer();
  
  Serial.print("Initializing SD card...");
  SPIClass spi_sd(VSPI);
  
  if (!SD.begin(5, spi_sd)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  readFile(SD, "/test.txt");


  //Adafruit_PN532& nfc = init_pn532();
  //dir = SD.open("/");
  player->play_folder_assync("/");
  //player->player_thread.join();

  while(true){
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);
    //Serial.println(F("Idling\n"));
  }
  // WARNING: if program reaches end of function app_main() the MCU will restart.
}
