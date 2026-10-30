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

#include "protocol_handler.h"
#include "ibmrseed.h"
#include "tn5250/protocol_constants.h"
#include "tn5250/telnet/commands.h"
#include "tn5250/telnet/options.h"

#include <cstdio>
#include <cstring>

namespace tn5250::client {

using telnet::TelnetCommand;
using telnet::TelnetOption;
using telnet::telnetCommandToString;
using telnet::telnetOptionToString;

ProtocolHandler::ProtocolHandler(const Config &config, const Callbacks &callbacks)
    : m_config(config), m_callbacks(callbacks), m_state(ConnectionState::Disconnected),
      m_inSubnegotiation(false), m_currentSubnegotiation(TelnetOption::TRANSMIT_BINARY),
      m_handshakeComplete(false), m_binaryNegotiated(false), m_eorNegotiated(false),
      m_terminalTypeSent(false) {}

void ProtocolHandler::setState(ConnectionState newState) {
    if (m_state != newState) {
        m_state = newState;
        if (m_callbacks.onStateChanged) m_callbacks.onStateChanged(newState);
    }
}

void ProtocolHandler::onSocketConnected() {
    setState(ConnectionState::Negotiating);
    performHandshake();
}

void ProtocolHandler::onBytesReceived(const std::vector<uint8_t> &data) {
    processTelnetData(data);
}

void ProtocolHandler::onSocketDisconnected() {
    setState(ConnectionState::Disconnected);
    if (m_callbacks.onDisconnected) m_callbacks.onDisconnected();
}

std::vector<uint8_t> ProtocolHandler::heartbeatPayload() const {
    return {static_cast<uint8_t>(TelnetCommand::IAC), static_cast<uint8_t>(TelnetCommand::NOP)};
}

// --- Telnet processing ---

void ProtocolHandler::processTelnetData(const std::vector<uint8_t> &data) {
    m_receiveBuffer.insert(m_receiveBuffer.end(), data.begin(), data.end());

    std::vector<uint8_t> processed;
    size_t i = 0;

    // Once EOR is negotiated, application bytes are collected per record and
    // handed over at IAC EOR (RFC 1205), so a record whose GDS length is
    // wrong cannot swallow the records that follow it.
    auto framing = [this]() { return m_handshakeComplete && m_eorNegotiated; };
    auto appendData = [&](uint8_t b) {
        if (framing()) {
            if (!processed.empty()) {
                m_recordBuffer.insert(m_recordBuffer.end(), processed.begin(), processed.end());
                processed.clear();
            }
            m_recordBuffer.push_back(b);
        } else {
            processed.push_back(b);
        }
    };

    while (i < m_receiveBuffer.size()) {
        uint8_t byte = m_receiveBuffer[i];

        if (byte == static_cast<uint8_t>(TelnetCommand::IAC)) {
            if (i + 1 >= m_receiveBuffer.size()) break;
            uint8_t next = m_receiveBuffer[i + 1];

            if (next == static_cast<uint8_t>(TelnetCommand::IAC)) {
                appendData(0xFF);
                i += 2;
                continue;
            }

            // Unescaped 0xFF in the data stream. Host Integration Server 2000
            // before the KB Q327297 hotfix forwards X'FF' bytes from the host's
            // GDS data without doubling them. Telnet commands are 0xEF (EOR)
            // and up, so any lower byte after IAC means the IAC was data.
            // Inside a record nothing but IAC IAC and IAC EOR is expected, so
            // there any other pair is data as well (e.g. 0xFF 0xF1).
            const bool midRecord = framing() && !m_recordBuffer.empty();
            if (!m_inSubnegotiation &&
                (next < static_cast<uint8_t>(TelnetCommand::EOR) ||
                 (midRecord && next != static_cast<uint8_t>(TelnetCommand::EOR)))) {
                if (m_callbacks.onLog) {
                    char msg[96];
                    std::snprintf(msg, sizeof(msg),
                                  "TN5250: unescaped 0xFF followed by 0x%02X, treating it as data", next);
                    m_callbacks.onLog(msg);
                }
                appendData(0xFF);
                i += 1;
                continue;
            }

            if (telnet::isStandaloneTelnetCommand(next)) {
                if (next == static_cast<uint8_t>(TelnetCommand::SE)) {
                    if (m_inSubnegotiation) {
                        handleSubnegotiation(m_currentSubnegotiation, m_subnegotiationBuffer);
                        m_subnegotiationBuffer.clear();
                        m_inSubnegotiation = false;
                    }
                } else if (next == static_cast<uint8_t>(TelnetCommand::EOR) && framing()) {
                    if (!processed.empty()) {
                        m_recordBuffer.insert(m_recordBuffer.begin(), processed.begin(), processed.end());
                        processed.clear();
                    }
                    flushRecord();
                }
                // Other standalone commands silently consumed
                i += 2;
                continue;
            }

            if (i + 2 >= m_receiveBuffer.size()) break;
            uint8_t cmd = next;
            uint8_t opt = m_receiveBuffer[i + 2];
            handleTelnetCommand(cmd, opt);
            i += 3;
        } else if (m_inSubnegotiation) {
            m_subnegotiationBuffer.push_back(byte);
            i++;
        } else {
            appendData(byte);
            i++;
        }
    }

    if (i > 0) {
        m_receiveBuffer.erase(m_receiveBuffer.begin(), m_receiveBuffer.begin() + i);
    }

    if (!processed.empty()) {
        if (m_handshakeComplete) {
            if (m_callbacks.onDataReceived) m_callbacks.onDataReceived(processed);
        } else {
            m_handshakeBuffer.insert(m_handshakeBuffer.end(), processed.begin(), processed.end());
            processHandshakeData(processed);
        }
    }
}

void ProtocolHandler::flushRecord() {
    if (m_recordBuffer.empty()) return;
    std::vector<uint8_t> record = repairGdsRecordLength(m_recordBuffer);
    if (record != m_recordBuffer && m_callbacks.onLog) {
        m_callbacks.onLog("TN5250: GDS record length does not match IAC EOR, using the EOR boundary");
    }
    m_recordBuffer.clear();
    if (m_callbacks.onDataReceived) m_callbacks.onDataReceived(record);
}

std::vector<uint8_t> ProtocolHandler::repairGdsRecordLength(const std::vector<uint8_t> &record) {
    using namespace tn5250::protocol;
    const size_t size = record.size();

    // Walk the chain of GDS length fields; a well-formed record (or several
    // back-to-back records) ends exactly at the EOR.
    size_t pos = 0;
    while (pos + 2 <= size) {
        const size_t len = (static_cast<size_t>(record[pos]) << 8) | record[pos + 1];
        if (len < static_cast<size_t>(GDS_MIN_RECORD_LEN) || pos + len > size) break;
        pos += len;
    }
    if (pos == size) return record;

    // The length disagrees with the EOR boundary (bytes lost or added in
    // transit). Trust the EOR so the decoder neither waits for bytes that
    // will never come nor reads into the next record.
    const size_t remaining = size - pos;
    if (remaining < static_cast<size_t>(GDS_HEADER_SIZE) ||
        remaining > static_cast<size_t>(GDS_MAX_RECORD_LEN) ||
        record[pos + 2] != GDS_RECORD_TYPE_HI || record[pos + 3] != GDS_RECORD_TYPE_LO) {
        return record;
    }
    std::vector<uint8_t> repaired = record;
    repaired[pos] = static_cast<uint8_t>((remaining >> 8) & 0xFF);
    repaired[pos + 1] = static_cast<uint8_t>(remaining & 0xFF);
    return repaired;
}

void ProtocolHandler::sendTelnetCommand(TelnetCommand cmd, TelnetOption opt) {
    std::vector<uint8_t> command = {
        static_cast<uint8_t>(TelnetCommand::IAC),
        static_cast<uint8_t>(cmd),
        static_cast<uint8_t>(opt)
    };
    if (m_callbacks.onSendBytes) m_callbacks.onSendBytes(command);
}

void ProtocolHandler::handleTelnetCommand(uint8_t cmd, uint8_t opt) {
    TelnetCommand command = static_cast<TelnetCommand>(cmd);
    TelnetOption option = static_cast<TelnetOption>(opt);

    switch (command) {
    case TelnetCommand::WILL:
        if (option == TelnetOption::TRANSMIT_BINARY) {
            sendTelnetCommand(TelnetCommand::DO, TelnetOption::TRANSMIT_BINARY);
            m_binaryNegotiated = true;
            checkHandshakeComplete();
        } else if (option == TelnetOption::END_OF_RECORD) {
            sendTelnetCommand(TelnetCommand::DO, TelnetOption::END_OF_RECORD);
            m_eorNegotiated = true;
            checkHandshakeComplete();
        } else if (option == TelnetOption::ECHO) {
            sendTelnetCommand(TelnetCommand::DO, TelnetOption::ECHO);
        } else if (option == TelnetOption::SUPPRESS_GO_AHEAD) {
            sendTelnetCommand(TelnetCommand::DO, TelnetOption::SUPPRESS_GO_AHEAD);
        } else {
            sendTelnetCommand(TelnetCommand::DONT, option);
        }
        break;

    case TelnetCommand::WONT:
        break;

    case TelnetCommand::DO:
        if (option == TelnetOption::TIMING_MARK) {
            sendTelnetCommand(TelnetCommand::WONT, TelnetOption::TIMING_MARK);
        } else if (option == TelnetOption::TRANSMIT_BINARY) {
            sendTelnetCommand(TelnetCommand::WILL, TelnetOption::TRANSMIT_BINARY);
            m_binaryNegotiated = true;
            checkHandshakeComplete();
        } else if (option == TelnetOption::END_OF_RECORD) {
            sendTelnetCommand(TelnetCommand::WILL, TelnetOption::END_OF_RECORD);
            m_eorNegotiated = true;
            checkHandshakeComplete();
        } else if (option == TelnetOption::TERMINAL_TYPE) {
            sendTelnetCommand(TelnetCommand::WILL, TelnetOption::TERMINAL_TYPE);
        } else if (option == TelnetOption::NEW_ENVIRON) {
            sendTelnetCommand(TelnetCommand::WILL, TelnetOption::NEW_ENVIRON);
        } else {
            sendTelnetCommand(TelnetCommand::WONT, option);
        }
        break;

    case TelnetCommand::DONT:
        break;

    case TelnetCommand::SB:
        m_inSubnegotiation = true;
        m_currentSubnegotiation = option;
        m_subnegotiationBuffer.clear();
        break;

    default:
        break;
    }
}

void ProtocolHandler::handleSubnegotiation(TelnetOption opt, const std::vector<uint8_t> &data) {
    switch (opt) {
    case TelnetOption::TERMINAL_TYPE:
        sendDeviceName();
        break;

    case TelnetOption::NEW_ENVIRON:
        if (!data.empty()) {
            uint8_t firstByte = data[0];
            if (firstByte == 0x01) { // SEND
                // Extract server seed from IBMRSEED variable
                m_serverSeed.clear();
                for (size_t si = 1; si < data.size(); ++si) {
                    uint8_t sb = data[si];
                    if (sb == 0x00 || sb == 0x03) {
                        size_t nameStart = si + 1;
                        size_t nameEnd = nameStart;
                        while (nameEnd < data.size()) {
                            uint8_t nb = data[nameEnd];
                            if (nb == 0x00 || nb == 0x01 || nb == 0x03) break;
                            nameEnd++;
                        }
                        std::vector<uint8_t> rawName(data.begin() + nameStart, data.begin() + nameEnd);
                        const std::string ibmrseedTag = "IBMRSEED";
                        if (rawName.size() > ibmrseedTag.size() &&
                            std::memcmp(rawName.data(), ibmrseedTag.data(), ibmrseedTag.size()) == 0) {
                            m_serverSeed.assign(rawName.begin() + ibmrseedTag.size(), rawName.end());
                        }
                        si = nameEnd - 1;
                    }
                }
            }
        }
        sendNewEnviron();
        break;

    default:
        break;
    }
}

void ProtocolHandler::sendData(const std::vector<uint8_t> &data) {
    using namespace tn5250::protocol;
    std::vector<uint8_t> record;
    int recLen = GDS_HEADER_SIZE + static_cast<int>(data.size());

    record.push_back(static_cast<uint8_t>((recLen >> 8) & 0xFF));
    record.push_back(static_cast<uint8_t>(recLen & 0xFF));
    record.push_back(GDS_RECORD_TYPE_HI);
    record.push_back(GDS_RECORD_TYPE_LO);
    record.push_back(0x00);
    record.push_back(0x00);
    record.push_back(GDS_VAR_HDR_LEN);
    record.push_back(0x00);
    record.push_back(0x00);
    record.push_back(GDS_OPCODE_PUT_GET);
    record.insert(record.end(), data.begin(), data.end());

    sendRawData(record);
}

void ProtocolHandler::sendGDS(uint8_t flagsHi, uint8_t opcode, const std::vector<uint8_t> &payload) {
    using namespace tn5250::protocol;
    std::vector<uint8_t> record;
    int recLen = GDS_HEADER_SIZE + static_cast<int>(payload.size());

    record.push_back(static_cast<uint8_t>((recLen >> 8) & 0xFF));
    record.push_back(static_cast<uint8_t>(recLen & 0xFF));
    record.push_back(GDS_RECORD_TYPE_HI);
    record.push_back(GDS_RECORD_TYPE_LO);
    record.push_back(0x00);
    record.push_back(0x00);
    record.push_back(GDS_VAR_HDR_LEN);
    record.push_back(flagsHi);
    record.push_back(0x00);
    record.push_back(opcode);
    record.insert(record.end(), payload.begin(), payload.end());

    sendRawData(record);
}

void ProtocolHandler::sendRawData(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> escaped;
    escaped.reserve(data.size() + 16);
    for (uint8_t byte : data) {
        if (byte == static_cast<uint8_t>(TelnetCommand::IAC)) {
            escaped.push_back(static_cast<uint8_t>(TelnetCommand::IAC));
            escaped.push_back(static_cast<uint8_t>(TelnetCommand::IAC));
        } else {
            escaped.push_back(byte);
        }
    }
    escaped.push_back(static_cast<uint8_t>(TelnetCommand::IAC));
    escaped.push_back(static_cast<uint8_t>(TelnetCommand::EOR));

    if (m_callbacks.onSendBytes) m_callbacks.onSendBytes(escaped);
}

// --- Handshake ---

void ProtocolHandler::performHandshake() {
    sendTelnetCommand(TelnetCommand::DO, TelnetOption::TRANSMIT_BINARY);
    sendTelnetCommand(TelnetCommand::WILL, TelnetOption::TRANSMIT_BINARY);
    sendTelnetCommand(TelnetCommand::DO, TelnetOption::END_OF_RECORD);
    sendTelnetCommand(TelnetCommand::WILL, TelnetOption::END_OF_RECORD);
}

void ProtocolHandler::sendDeviceName() {
    std::vector<uint8_t> negotiation;
    negotiation.push_back(static_cast<uint8_t>(TelnetCommand::IAC));
    negotiation.push_back(static_cast<uint8_t>(TelnetCommand::SB));
    negotiation.push_back(static_cast<uint8_t>(TelnetOption::TERMINAL_TYPE));
    negotiation.push_back(0x00); // IS
    for (char c : m_config.terminalType) negotiation.push_back(static_cast<uint8_t>(c));
    negotiation.push_back(static_cast<uint8_t>(TelnetCommand::IAC));
    negotiation.push_back(static_cast<uint8_t>(TelnetCommand::SE));

    if (m_callbacks.onSendBytes) m_callbacks.onSendBytes(negotiation);
    m_terminalTypeSent = true;

    if (!m_binaryNegotiated) m_binaryNegotiated = true;
    checkHandshakeComplete();
}

void ProtocolHandler::sendNewEnviron() {
    std::vector<uint8_t> negotiation;
    negotiation.push_back(static_cast<uint8_t>(TelnetCommand::IAC));
    negotiation.push_back(static_cast<uint8_t>(TelnetCommand::SB));
    negotiation.push_back(static_cast<uint8_t>(TelnetOption::NEW_ENVIRON));
    negotiation.push_back(0x00); // IS

    // DEVNAME
    negotiation.push_back(0x03); // USERVAR
    for (char c : std::string("DEVNAME")) negotiation.push_back(static_cast<uint8_t>(c));
    negotiation.push_back(0x01); // VALUE
    for (char c : m_config.deviceName) negotiation.push_back(static_cast<uint8_t>(c));

    // IBMRSEED / IBMSUBSPW
    bool encrypted = false;
    tn5250::codepage::CodePage cp(m_config.codePage);
    if (m_serverSeed.size() == 8 && !m_config.username.empty() && !m_config.password.empty()) {
        auto clientSeed = IBMRSeed::generateClientSeed();
        auto pwSub = IBMRSeed::encryptPassword(m_config.username, m_config.password,
                                                m_serverSeed, clientSeed, cp);
        if (!pwSub.empty()) {
            encrypted = true;
            negotiation.push_back(0x03);
            for (char c : std::string("IBMRSEED")) negotiation.push_back(static_cast<uint8_t>(c));
            negotiation.push_back(0x01);
            auto escapedSeed = IBMRSeed::escapeNewEnviron(clientSeed);
            negotiation.insert(negotiation.end(), escapedSeed.begin(), escapedSeed.end());

            negotiation.push_back(0x03);
            for (char c : std::string("IBMSUBSPW")) negotiation.push_back(static_cast<uint8_t>(c));
            negotiation.push_back(0x01);
            auto escapedPw = IBMRSeed::escapeNewEnviron(pwSub);
            negotiation.insert(negotiation.end(), escapedPw.begin(), escapedPw.end());
        }
    }
    if (!encrypted) {
        negotiation.push_back(0x03);
        for (char c : std::string("IBMRSEED")) negotiation.push_back(static_cast<uint8_t>(c));
        negotiation.push_back(0x01);
    }

    // IBMSUBSVAR
    negotiation.push_back(0x03);
    for (char c : std::string("IBMSUBSVAR")) negotiation.push_back(static_cast<uint8_t>(c));
    negotiation.push_back(0x01);

    // KBDTYPE
    negotiation.push_back(0x03);
    for (char c : std::string("KBDTYPE")) negotiation.push_back(static_cast<uint8_t>(c));
    negotiation.push_back(0x01);
    for (char c : std::string("USB")) negotiation.push_back(static_cast<uint8_t>(c));

    // CODEPAGE
    negotiation.push_back(0x03);
    for (char c : std::string("CODEPAGE")) negotiation.push_back(static_cast<uint8_t>(c));
    negotiation.push_back(0x01);
    auto cpStr = std::to_string(static_cast<int>(m_config.codePage));
    for (char c : cpStr) negotiation.push_back(static_cast<uint8_t>(c));

    // CHARSET
    negotiation.push_back(0x03);
    for (char c : std::string("CHARSET")) negotiation.push_back(static_cast<uint8_t>(c));
    negotiation.push_back(0x01);
    for (char c : std::string("697")) negotiation.push_back(static_cast<uint8_t>(c));

    negotiation.push_back(static_cast<uint8_t>(TelnetCommand::IAC));
    negotiation.push_back(static_cast<uint8_t>(TelnetCommand::SE));

    if (m_callbacks.onSendBytes) m_callbacks.onSendBytes(negotiation);
}

void ProtocolHandler::processHandshakeData(const std::vector<uint8_t> &data) {
    if (!m_handshakeComplete && !data.empty()) {
        if (!m_binaryNegotiated) m_binaryNegotiated = true;
        checkHandshakeComplete();
    }
}

void ProtocolHandler::checkHandshakeComplete() {
    if (!m_handshakeComplete && m_binaryNegotiated) {
        bool shouldComplete = false;
        if (m_terminalTypeSent) {
            shouldComplete = true;
        } else if (!m_handshakeBuffer.empty()) {
            shouldComplete = true;
        }
        if (shouldComplete) {
            m_handshakeComplete = true;
            setState(ConnectionState::Connected);
            if (m_callbacks.onConnected) m_callbacks.onConnected();
            if (!m_handshakeBuffer.empty()) {
                if (m_callbacks.onDataReceived) m_callbacks.onDataReceived(m_handshakeBuffer);
                m_handshakeBuffer.clear();
            }
        }
    }
}

} // namespace tn5250::client
