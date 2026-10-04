#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace boubox {

// Extracts the text stored on an NFC Forum Type 2 tag (NTAG2xx) from the raw
// user memory (starting at page 4).
// - An NDEF message containing a "Text" record (UTF-8) is decoded.
// - Otherwise, if the memory does not start with a TLV, it is interpreted as a
//   raw printable ASCII string terminated by NUL/0xFF.
std::optional<std::string> ExtractTagText(const uint8_t* data, size_t len);

}  // namespace boubox
