
#include "nfc_reader.hpp"

#include <Arduino.h>
#include <Wire.h>


#include <SPI.h>
#include <SD.h>

File myFile;


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

  Serial.print("Initializing SD card...");
  SPIClass spi_sd(VSPI);

  if (!SD.begin(5, spi_sd)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  readFile(SD, "/test.txt");


  Adafruit_PN532& nfc = init_pn532();

  // Arduino-like loop()
  while(true){
    const nfc_data_t<10> nfc_data = read_nfc<10>(nfc);
    Serial.print("Block with n pages ");
    Serial.print(nfc_data.n_pages_read);
    Serial.println();
    for (size_t i = 0; i < nfc_data.n_pages_read; i++)
    {
      Serial.print(i);
      Serial.print(" ");
      Serial.print(nfc_data.data[i], HEX);
      Serial.print(nfc_data.data[i+1], HEX);
      Serial.print(nfc_data.data[i+2], HEX);
      Serial.print(nfc_data.data[i+3], HEX);
      Serial.println();
    }
    
        // Wait a bit before trying again
    Serial.println("\n\nDone, will retry in 3 seconds!");
    delay(3000);
  }

  // WARNING: if program reaches end of function app_main() the MCU will restart.
}
