#pragma once

#include <functional>
#include <string>

#include "esp_err.h"
#include "pn532.hpp"

namespace boubox {

// Polls the PN532 and reports the text stored on a tag (the folder name) each
// time a tag is presented. A tag left on the reader is reported only once.
class TagScanner
{
public:
    using Callback = std::function<void(const std::string& text)>;

    esp_err_t Start(const Pn532::Config& pn532_config, uint32_t poll_period_ms, Callback callback);

private:
    static void TaskEntry(void* arg);
    void Run();
    bool ReadText(std::string* text);

    Pn532 pn532_;
    Pn532::Config pn532_config_{};
    uint32_t poll_period_ms_ = 250;
    Callback callback_;
};

}  // namespace boubox
