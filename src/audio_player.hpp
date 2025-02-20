#pragma once
#include <AudioFileSourceSD.h>
#include <AudioOutputI2S.h>

#include <AudioGeneratorWAV.h>
#include <FS.h>

#include <thread>

class AudioPlayer {
public:
    AudioFileSourceSD source;
    AudioOutputI2S out;
    AudioGeneratorWAV decoder;
    std::thread player_thread;
    bool playing = false;

    AudioPlayer() : source(), out(0, 1), decoder() {}
    ~AudioPlayer() {
        playing = false;
        if (player_thread.joinable()) {
            player_thread.join();
        }
    }
    void play_folder(fs::File& dir) {
        playing = true;
        player_thread = std::thread([this, &dir] {
            fs::File file = dir.openNextFile();
            if(file)
            {
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
        });
    }

};