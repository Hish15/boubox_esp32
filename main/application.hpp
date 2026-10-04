#pragma once

#include "audio_player.hpp"
#include "buttons.hpp"
#include "sd_card.hpp"
#include "tag_scanner.hpp"

namespace boubox {

class Application
{
public:
    // Starts all the services. Returns an error if the music box cannot operate.
    esp_err_t Start();

private:
    void OnButton(Button button);
    void OnTag(const std::string& folder);

    SdCard sd_card_;
    AudioPlayer player_;
    TagScanner tag_scanner_;
    Buttons buttons_;
};

}  // namespace boubox
