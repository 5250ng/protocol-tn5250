// protocol-tn5250 - Standalone TN5250 protocol library
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tn5250::client {

// TN5250 command codes (based on RFC 1205 and IBM documentation)
enum class TN5250Command : uint8_t {
    WRITE_STRUCTURED_FIELD = 0x11,
    ERASE_WRITE = 0x05,
    ERASE_WRITE_ALTERNATE = 0x0D,
    READ_MODIFY = 0x06,
    READ_MODIFY_WRITE = 0x07,
    READ_MDT_FIELDS = 0x52,
    READ_INPUT_FIELDS = 0x42,
    READ_IMMEDIATE = 0x72
};

// TN5250 structured field types
enum class StructuredFieldType : uint8_t {
    OUTBOUND_3270DS = 0x00,
    SCS = 0x01,
    OUTBOUND_5250_DS = 0x02,
    INBOUND_3270DS = 0x80,
    INBOUND_5250_DS = 0x82
};

// Minimal parser state
enum class ParserState {
    WaitingForCommand,
    ReadingLength,
    ReadingData,
    Complete
};

// Callback struct for decoder events (replaces Qt signals)
struct DecoderCallbacks {
    std::function<void(TN5250Command, const std::vector<uint8_t> &)> onCommandReceived;
    std::function<void(StructuredFieldType, const std::vector<uint8_t> &)> onStructuredFieldReceived;
    std::function<void(const std::vector<uint8_t> &)> onRawScreenData;
    std::function<void()> onClearScreen;
    std::function<void()> onClearScreenAlternate;
    std::function<void()> onKeyboardUnlock;
    std::function<void(uint8_t, uint8_t)> onControlCharacters;
    std::function<void(uint8_t, uint8_t, uint8_t, uint8_t)> onSoh;
    std::function<void(uint8_t, uint8_t, uint8_t, bool)> onRoll;
    std::function<void(const std::vector<uint8_t> &)> onWriteErrorCode;
    std::function<void()> onSaveScreen;
    std::function<void()> onClearFormatTable;
    std::function<void()> onInvite;
    std::function<void()> onCancelInvite;
    std::function<void()> onMessageLightOn;
    std::function<void()> onMessageLightOff;
    std::function<void(bool)> onReadScreen;
    std::function<void(const std::vector<uint8_t> &)> onWriteStructuredField;
    // Fires when a Write-To-Display stream contains the STRPCCMD trigger byte
    // (0x80) followed by the fixed 9-byte PCO signature. The decoder consumes
    // and removes the 10-byte marker from the rendered display data so it does
    // not leak into onRawScreenData. The actual command string is not parsed
    // here — it lives at fixed screen coordinates the host writes via normal
    // SBA + text orders, and the consumer reads it off the rendered screen.
    std::function<void()> onStrpccmdRequested;
    std::function<void(const std::string &)> onParseError;
    std::function<void(const std::string &)> onLog;
};

// Application-layer TN5250 decoder (post-Telnet)
class Decoder {
  public:
    explicit Decoder(const DecoderCallbacks &callbacks = {});

    void parseData(const std::vector<uint8_t> &data);
    void reset();
    ParserState state() const { return m_state; }

    DecoderCallbacks &callbacks() { return m_callbacks; }

  private:
    ParserState m_state;
    std::vector<uint8_t> m_buffer;
    DecoderCallbacks m_callbacks;
};

} // namespace tn5250::client
