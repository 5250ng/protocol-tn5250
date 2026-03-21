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

#include "client.h"

namespace telnet {

Client::Client()
    : m_state(State::Data), m_pendingCmd(TelnetCommand::IAC),
      m_pendingOpt(TelnetOption::TRANSMIT_BINARY),
      m_sbOpt(TelnetOption::TRANSMIT_BINARY) {}

void Client::setAppDataCallback(std::function<void(const std::vector<uint8_t> &)> cb) {
    m_onAppData = std::move(cb);
}

void Client::flushAppData() {
    if (!m_appBuffer.empty() && m_onAppData) {
        m_onAppData(m_appBuffer);
    }
    m_appBuffer.clear();
}

void Client::reset() {
    m_state = State::Data;
    m_appBuffer.clear();
    m_sbBuffer.clear();
}

void Client::feed(const std::vector<uint8_t> &data) {
    for (uint8_t b : data) {
        switch (m_state) {
        case State::Data:
            if (b == static_cast<uint8_t>(TelnetCommand::IAC)) {
                flushAppData();
                m_state = State::IAC;
            } else {
                m_appBuffer.push_back(b);
            }
            break;

        case State::IAC: {
            TelnetCommand cmd = static_cast<TelnetCommand>(b);
            if (cmd == TelnetCommand::IAC) {
                m_appBuffer.push_back(0xFF);
                m_state = State::Data;
            } else if (cmd == TelnetCommand::SB) {
                m_state = State::SubnegotiationOption;
            } else if (cmd == TelnetCommand::SE) {
                m_state = State::Data;
            } else if (isStandaloneTelnetCommand(cmd)) {
                if (onStandaloneCommand) onStandaloneCommand(cmd);
                m_state = State::Data;
            } else {
                m_pendingCmd = cmd;
                m_state = State::NegotiationOption;
            }
        } break;

        case State::NegotiationOption: {
            m_pendingOpt = static_cast<TelnetOption>(b);
            if (onNegotiationCommand) onNegotiationCommand(m_pendingCmd, m_pendingOpt);
            m_state = State::Data;
        } break;

        case State::SubnegotiationOption: {
            m_sbOpt = static_cast<TelnetOption>(b);
            m_sbBuffer.clear();
            m_state = State::SubnegotiationData;
        } break;

        case State::SubnegotiationData:
            if (b == static_cast<uint8_t>(TelnetCommand::IAC)) {
                m_state = State::SubnegotiationIAC;
            } else {
                m_sbBuffer.push_back(b);
            }
            break;

        case State::SubnegotiationIAC: {
            TelnetCommand sbCmd = static_cast<TelnetCommand>(b);
            if (sbCmd == TelnetCommand::IAC) {
                m_sbBuffer.push_back(0xFF);
                m_state = State::SubnegotiationData;
            } else if (sbCmd == TelnetCommand::SE) {
                if (onSubnegotiation) onSubnegotiation(m_sbOpt, m_sbBuffer);
                m_sbBuffer.clear();
                m_state = State::Data;
            } else {
                m_state = State::SubnegotiationData;
            }
        } break;
        }
    }
    flushAppData();
}

} // namespace telnet
