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

#include "decoder.h"
#include "tn5250/protocol_constants.h"

#include <algorithm>
#include <cstdio>

namespace tn5250::client {

Decoder::Decoder(const DecoderCallbacks &callbacks)
    : m_state(ParserState::WaitingForCommand), m_callbacks(callbacks) {}

void Decoder::parseData(const std::vector<uint8_t> &data) {
    m_buffer.insert(m_buffer.end(), data.begin(), data.end());

    using namespace tn5250::protocol;
    while (true) {
        if (static_cast<int>(m_buffer.size()) < GDS_MIN_RECORD_LEN) break;

        const uint8_t b0 = m_buffer[0];
        const uint8_t b1 = m_buffer[1];
        const int recLen = (static_cast<int>(b0) << 8) | static_cast<int>(b1);

        if (recLen < GDS_MIN_RECORD_LEN || recLen > GDS_MAX_RECORD_LEN) {
            m_buffer.erase(m_buffer.begin());
            continue;
        }
        if (static_cast<int>(m_buffer.size()) < recLen) break;

        std::vector<uint8_t> rec(m_buffer.begin(), m_buffer.begin() + recLen);
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + recLen);

        if (static_cast<int>(rec.size()) < 7) {
            if (m_callbacks.onParseError) m_callbacks.onParseError("TN5250: record too short");
            continue;
        }

        const uint8_t r2 = rec[2];
        const uint8_t r3 = rec[3];
        if (!(r2 == GDS_RECORD_TYPE_HI && r3 == GDS_RECORD_TYPE_LO)) {
            if (m_callbacks.onRawScreenData) m_callbacks.onRawScreenData(rec);
            continue;
        }

        const int varHdrStart = 6;
        const int varLen = rec[varHdrStart];
        if (static_cast<int>(rec.size()) < varHdrStart + 1 + varLen) {
            if (m_callbacks.onParseError) m_callbacks.onParseError("TN5250: incomplete variable header");
            continue;
        }
        if (varLen < 4) {
            if (m_callbacks.onParseError) m_callbacks.onParseError("TN5250: variable header too short");
            continue;
        }

        [[maybe_unused]] const uint8_t flagsHi = rec[varHdrStart + 1];
        [[maybe_unused]] const uint8_t flagsLo = rec[varHdrStart + 2];
        const uint8_t opcode = rec[varHdrStart + 3];

        const int payloadStart = varHdrStart + varLen;
        int payloadLen = static_cast<int>(rec.size()) - payloadStart;
        if (payloadLen < 0) payloadLen = 0;
        std::vector<uint8_t> payload(rec.begin() + payloadStart, rec.begin() + payloadStart + payloadLen);

        if (opcode == GDS_OPCODE_SAVE_SCREEN) {
            if (m_callbacks.onSaveScreen) m_callbacks.onSaveScreen();
            // Fall through to process payload commands (e.g. Clear Unit, Write To Display)
            // which may unlock the keyboard or update the screen.
        }
        if (opcode == GDS_OPCODE_INVITE) {
            if (m_callbacks.onInvite) m_callbacks.onInvite();
            continue;
        }
        if (opcode == GDS_OPCODE_CANCEL_INVITE) {
            if (m_callbacks.onCancelInvite) m_callbacks.onCancelInvite();
            continue;
        }
        if (opcode == GDS_OPCODE_MSG_LIGHT_ON) {
            if (m_callbacks.onMessageLightOn) m_callbacks.onMessageLightOn();
            continue;
        }
        if (opcode == GDS_OPCODE_MSG_LIGHT_OFF) {
            if (m_callbacks.onMessageLightOff) m_callbacks.onMessageLightOff();
            continue;
        }

        if (opcode == GDS_OPCODE_OUTPUT_ONLY || opcode == GDS_OPCODE_PUT_GET || opcode == GDS_OPCODE_RESTORE || opcode == GDS_OPCODE_SAVE_SCREEN || opcode == GDS_OPCODE_READ_SCREEN) {
            std::vector<uint8_t> display;
            for (int i = 0; i < static_cast<int>(payload.size());) {
                uint8_t ch = payload[i];
                if (ch == ESC) {
                    if (i + 1 >= static_cast<int>(payload.size())) break;
                    uint8_t cc = payload[i + 1];

                    if (cc == CC_CLEAR_UNIT) {
                        if (m_callbacks.onClearScreen) m_callbacks.onClearScreen();
                        i += 2; continue;
                    }
                    if (cc == CC_CLEAR_UNIT_ALTERNATE) {
                        if (m_callbacks.onClearScreenAlternate) m_callbacks.onClearScreenAlternate();
                        i += 2; continue;
                    }
                    if (cc == CC_WRITE_ERROR_CODE) {
                        int j = i + 2;
                        std::vector<uint8_t> errData;
                        while (j < static_cast<int>(payload.size())) {
                            if (payload[j] == ESC) break;
                            errData.push_back(payload[j]);
                            j++;
                        }
                        if (m_callbacks.onWriteErrorCode) m_callbacks.onWriteErrorCode(errData);
                        i = j; continue;
                    }
                    if (cc == CC_ROLL) {
                        if (i + 3 < static_cast<int>(payload.size())) {
                            uint8_t ctrl1 = payload[i + 2];
                            uint8_t ctrl2 = payload[i + 3];
                            bool up = (ctrl1 & 0x80) != 0;
                            uint8_t lineCount = ctrl2 & 0x1F;
                            uint8_t topRow = 0, botRow = 0;
                            if (i + 5 < static_cast<int>(payload.size())) {
                                topRow = payload[i + 4] - 1;
                                botRow = payload[i + 5] - 1;
                                i += 6;
                            } else {
                                i += 4;
                            }
                            if (m_callbacks.onRoll) m_callbacks.onRoll(topRow, botRow, lineCount, up);
                        } else {
                            i += 4;
                        }
                        continue;
                    }
                    if (cc == CC_WRITE_ERROR_CODE_TO_WINDOW) {
                        int j = i + 2;
                        std::vector<uint8_t> errData;
                        while (j < static_cast<int>(payload.size())) {
                            if (payload[j] == ESC) break;
                            errData.push_back(payload[j]);
                            j++;
                        }
                        if (m_callbacks.onWriteErrorCode) m_callbacks.onWriteErrorCode(errData);
                        i = j; continue;
                    }
                    if (cc == CC_READ_INPUT_FIELDS) {
                        if (m_callbacks.onCommandReceived)
                            m_callbacks.onCommandReceived(TN5250Command::READ_INPUT_FIELDS, {});
                        i += 4; continue;
                    }
                    if (cc == CC_CLEAR_FORMAT_TABLE) {
                        if (m_callbacks.onClearFormatTable) m_callbacks.onClearFormatTable();
                        i += 2; continue;
                    }
                    if (cc == CC_READ_MDT_FIELDS) {
                        if (m_callbacks.onCommandReceived)
                            m_callbacks.onCommandReceived(TN5250Command::READ_MDT_FIELDS, {});
                        i += 4; continue;
                    }
                    if (cc == CC_READ_IMMEDIATE) {
                        if (m_callbacks.onCommandReceived)
                            m_callbacks.onCommandReceived(TN5250Command::READ_IMMEDIATE, {});
                        i += 4; continue;
                    }
                    if (cc == CC_READ_SCREEN || cc == CC_READ_SCREEN_ALT) {
                        bool includeAttrs = (cc == CC_READ_SCREEN_ALT);
                        if (m_callbacks.onReadScreen) m_callbacks.onReadScreen(includeAttrs);
                        i += 4; continue;
                    }
                    if (cc == CC_WRITE_STRUCTURED_FIELD) {
                        // RFC 1205 §12.5.1: a WSF stream contains one or more
                        // structured fields, each prefixed with a big-endian
                        // 2-byte length covering the entire SF (length + class
                        // + type + data). SF data is binary and may contain
                        // 0x04 bytes, so we must not use ESC as a terminator.
                        // Walk every SF using its length, stop at the next ESC
                        // (start of another command) or end of payload, and
                        // hand the raw SF bytes to the caller.
                        int j = i + 2;
                        const int wsfStart = j;
                        while (j + 1 < static_cast<int>(payload.size())) {
                            if (payload[j] == ESC) break;
                            const int sfLen = (static_cast<int>(payload[j]) << 8) |
                                              static_cast<int>(payload[j + 1]);
                            if (sfLen < 2 || j + sfLen > static_cast<int>(payload.size())) {
                                if (m_callbacks.onParseError)
                                    m_callbacks.onParseError("TN5250: WSF structured-field length out of range");
                                j = payload.size();
                                break;
                            }
                            j += sfLen;
                        }
                        if (m_callbacks.onWriteStructuredField) {
                            std::vector<uint8_t> sfData(payload.begin() + wsfStart,
                                                        payload.begin() + j);
                            m_callbacks.onWriteStructuredField(sfData);
                        }
                        i = j; continue;
                    }
                    if (cc == CC_WRITE_TO_DISPLAY) {
                        if (i + 3 >= static_cast<int>(payload.size())) break;
                        uint8_t ctrl1 = payload[i + 2];
                        uint8_t ctrl2 = payload[i + 3];
                        if (m_callbacks.onControlCharacters) m_callbacks.onControlCharacters(ctrl1, ctrl2);
                        if (ctrl2 & 0x08) {
                            if (m_callbacks.onKeyboardUnlock) m_callbacks.onKeyboardUnlock();
                        }
                        int j = i + 4;
                        while (j < static_cast<int>(payload.size())) {
                            uint8_t ob = payload[j];
                            if (ob == ESC) break;
                            if (ob == SOH) {
                                if (j + 1 >= static_cast<int>(payload.size())) { j = payload.size(); break; }
                                uint8_t sohLen = payload[j + 1];
                                if (sohLen >= 4) {
                                    uint8_t errorRow = (j + 5 < static_cast<int>(payload.size())) ? payload[j + 5] : 0;
                                    uint8_t ckm1 = (sohLen >= 5 && j + 6 < static_cast<int>(payload.size())) ? payload[j + 6] : 0;
                                    uint8_t ckm2 = (sohLen >= 6 && j + 7 < static_cast<int>(payload.size())) ? payload[j + 7] : 0;
                                    uint8_t ckm3 = (sohLen >= 7 && j + 8 < static_cast<int>(payload.size())) ? payload[j + 8] : 0;
                                    if (m_callbacks.onSoh) m_callbacks.onSoh(errorRow, ckm1, ckm2, ckm3);
                                }
                                j += 2 + sohLen;
                                if (j > static_cast<int>(payload.size())) j = payload.size();
                                continue;
                            }
                            if (ob == ORDER_SBA || ob == ORDER_IC || ob == ORDER_MC) {
                                int n = std::min(3, static_cast<int>(payload.size()) - j);
                                display.insert(display.end(), payload.begin() + j, payload.begin() + j + n);
                                j += n; continue;
                            }
                            if (ob == ORDER_RA || ob == ORDER_EA) {
                                int n = std::min(4, static_cast<int>(payload.size()) - j);
                                display.insert(display.end(), payload.begin() + j, payload.begin() + j + n);
                                j += n; continue;
                            }
                            if (ob == ORDER_TD) {
                                if (j + 1 < static_cast<int>(payload.size())) {
                                    int tdLen = payload[j + 1];
                                    int n = std::min(2 + tdLen, static_cast<int>(payload.size()) - j);
                                    display.insert(display.end(), payload.begin() + j, payload.begin() + j + n);
                                    j += n;
                                } else {
                                    display.push_back(ob);
                                    j++;
                                }
                                continue;
                            }
                            if (ob == ORDER_WDSF) {
                                if (j + 2 < static_cast<int>(payload.size())) {
                                    int wdsfLen = (static_cast<int>(payload[j + 1]) << 8) | static_cast<int>(payload[j + 2]);
                                    if (wdsfLen < 2) wdsfLen = 2;
                                    j += 1 + wdsfLen;
                                    if (j > static_cast<int>(payload.size())) j = payload.size();
                                } else {
                                    j = payload.size();
                                }
                                continue;
                            }
                            if (ob == STRPCCMD_TRIGGER &&
                                j + STRPCCMD_MARKER_LEN <= static_cast<int>(payload.size()) &&
                                std::equal(STRPCCMD_SIGNATURE,
                                           STRPCCMD_SIGNATURE + sizeof(STRPCCMD_SIGNATURE),
                                           payload.begin() + j + 1)) {
                                // STRPCCMD marker: 0x80 trigger + 9-byte PCO
                                // signature, followed by a 1-byte wait flag and
                                // up to 123 EBCDIC command bytes per
                                // SA21-9247-6 §15.7. Reading the command from
                                // the wire here avoids the screen-buffer
                                // round-trip the consumer would otherwise need
                                // (which is fragile across OS/400 versions —
                                // V5R4 places the bytes inline rather than at
                                // tn5250j's hard-coded position 11).
                                int p = j + STRPCCMD_MARKER_LEN;
                                bool noWait = false;
                                std::vector<uint8_t> commandBytes;
                                if (p < static_cast<int>(payload.size()) &&
                                    payload[p] != ESC) {
                                    // EBCDIC 'a' = 0x81 → no-wait; any other
                                    // non-ESC byte means wait. Treat ESC at the
                                    // wait-flag position as "no payload";
                                    // commandBytes remains empty and the
                                    // consumer can decide how to handle that.
                                    noWait = (payload[p] == 0x81);
                                    p++;
                                    // Read up to STRPCCMD_MAX_COMMAND_LEN
                                    // bytes, but stop at the first byte < 0x40.
                                    // Printable EBCDIC always lies in 0x40..0xFE;
                                    // any byte below that boundary is either an
                                    // EBCDIC control character or the first byte
                                    // of the next 5250 order (e.g. RA = 0x02,
                                    // SBA = 0x11, SF = 0x1D). OS/400 V4R5 and
                                    // V5R4 do not pad the PCCMD region — they
                                    // write the next order immediately after the
                                    // command — so the 0x40 floor is the only
                                    // reliable end-of-command signal.
                                    const int payloadEnd =
                                        std::min(p + STRPCCMD_MAX_COMMAND_LEN,
                                                 static_cast<int>(payload.size()));
                                    while (p < payloadEnd && payload[p] >= 0x40) {
                                        commandBytes.push_back(payload[p]);
                                        p++;
                                    }
                                }
                                if (m_callbacks.onStrpccmdRequested) {
                                    m_callbacks.onStrpccmdRequested(noWait, commandBytes);
                                }
                                j = p;
                                continue;
                            }
                            if (ob == ORDER_SF) {
                                // SF order: 0x1D [FFW1] [FFW2] [FCW pairs...] [attr] [len_hi] [len_lo]
                                // The 2-byte length specifies the field size on screen,
                                // NOT a count of trailing data bytes in the stream.
                                int k = j + 1;
                                if (k + 1 < static_cast<int>(payload.size())) k += 2; // FFW1+FFW2
                                else { k = payload.size(); }
                                while (k + 1 < static_cast<int>(payload.size()) &&
                                       (payload[k] & 0xE0) != 0x20) {
                                    k += 2; // skip FCW pairs
                                }
                                if (k < static_cast<int>(payload.size())) k++; // attr byte
                                if (k + 1 < static_cast<int>(payload.size())) {
                                    k += 2; // 2-byte field length (screen positions, not data bytes)
                                }
                                if (k > static_cast<int>(payload.size())) k = payload.size();
                                int n = k - j;
                                display.insert(display.end(), payload.begin() + j, payload.begin() + j + n);
                                j = k; continue;
                            }
                            display.push_back(ob);
                            j++;
                        }
                        i = j;
                        continue;
                    }
                    // Unknown ESC command. We do not know its payload length,
                    // so resync by advancing to the next ESC byte (start of
                    // another command) or to the end of the payload. This
                    // avoids leaking the unknown command's payload bytes into
                    // the display vector via the loose-byte fallthrough below,
                    // which would otherwise be re-parsed as 5250 orders by the
                    // downstream renderer and corrupt the screen.
                    if (m_callbacks.onParseError) {
                        char buf[64];
                        std::snprintf(buf, sizeof(buf),
                                      "TN5250: unknown ESC command 0x%02X", cc);
                        m_callbacks.onParseError(buf);
                    }
                    int k = i + 2;
                    while (k < static_cast<int>(payload.size()) &&
                           payload[k] != ESC) {
                        k++;
                    }
                    i = k;
                    continue;
                }
                if (ch == SOH) {
                    if (i + 1 >= static_cast<int>(payload.size())) break;
                    uint8_t sohLen = payload[i + 1];
                    if (sohLen >= 4) {
                        uint8_t errorRow = (i + 5 < static_cast<int>(payload.size())) ? payload[i + 5] : 0;
                        uint8_t ckm1 = (sohLen >= 5 && i + 6 < static_cast<int>(payload.size())) ? payload[i + 6] : 0;
                        uint8_t ckm2 = (sohLen >= 6 && i + 7 < static_cast<int>(payload.size())) ? payload[i + 7] : 0;
                        uint8_t ckm3 = (sohLen >= 7 && i + 8 < static_cast<int>(payload.size())) ? payload[i + 8] : 0;
                        if (m_callbacks.onSoh) m_callbacks.onSoh(errorRow, ckm1, ckm2, ckm3);
                    }
                    i += 2 + sohLen;
                    if (i > static_cast<int>(payload.size())) i = payload.size();
                    continue;
                }
                // A non-ESC, non-SOH byte at this level is not part of any
                // recognised command — display data lives inside the WTD/WEC
                // inner loops, not at the top level. Skip the byte rather than
                // emitting it as raw screen data, which would be re-parsed as
                // 5250 orders and corrupt the rendered screen.
                if (m_callbacks.onParseError) {
                    char buf[64];
                    std::snprintf(buf, sizeof(buf),
                                  "TN5250: stray byte 0x%02X outside any command",
                                  static_cast<unsigned>(ch));
                    m_callbacks.onParseError(buf);
                }
                i++;
            }
            // Always emit raw screen data (even when empty) so that
            // deferred CC2 processing is triggered in the command handler.
            // A WTD with CC2 unlock bit but no display orders would otherwise
            // leave the keyboard permanently locked.
            if (m_callbacks.onRawScreenData) m_callbacks.onRawScreenData(display);
            continue;
        }
    }
}

void Decoder::reset() { m_state = ParserState::WaitingForCommand; }

} // namespace tn5250::client
