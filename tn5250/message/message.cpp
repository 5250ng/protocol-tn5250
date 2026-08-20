// 5250ng - A modern IBM TN5250 terminal emulator
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

#include "message.h"

namespace tn5250::message {

/**
 * Unmarshal a 5250 message from buffer starting at offset 0.
 *
 * @param buffer Input bytes containing a single GDS record (length-prefixed).
 * @param out    Output message.
 * @param error  Optional error string on failure.
 * @return true on success, false otherwise.
 */
uint32_t Message::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    // Unmarshal the fixed header
    uint32_t read_bytes = header.unmarshal(buffer, error);
    if (read_bytes == 0) {
        return 0;
    }

    // The fixed header guard only proves read_bytes bytes are present.
    // Guarantee the variableLength byte is also present before reading it.
    if (read_bytes >= buffer.size()) {
        if (error)
            *error = "5250Message: buffer too short for variable-length byte";
        return 0;
    }

    // The variable-header length includes its own length byte.
    variableLength = static_cast<uint8_t>(buffer[read_bytes]);
    read_bytes++;

    if (variableLength < 4) {
        if (error)
            *error = "5250Message: variable header too short";
        return 0;
    }

    // Verify we have enough bytes for variable header
    const size_t needed = static_cast<size_t>(read_bytes) + variableLength - 1;
    if (buffer.size() < needed) {
        if (error)
            *error = "5250Message: buffer too short for variable header";
        return 0;
    }

    snaFlags = static_cast<uint8_t>(buffer[read_bytes++]);
    variableReserved = static_cast<uint8_t>(buffer[read_bytes++]);
    opcode = OperationCode(static_cast<uint8_t>(buffer[read_bytes++]));

    // Skip any extension bytes in a variable header longer than the standard
    // four-byte layout before parsing the command payload.
    read_bytes = static_cast<uint32_t>(needed);

    // Unmarshal the commands
    while (read_bytes < buffer.size()) {
        const std::vector<uint8_t> sub(buffer.begin() + read_bytes, buffer.end());

        if (sub.empty()) {
            if (error) {
                *error = "5250Message: unexpected end of buffer while parsing commands";
            }
            return 0;
        }
        command::Command c;
        uint32_t bytesRead = command::unmarshalCommand(sub, c, error);
        read_bytes += bytesRead;
        if (bytesRead == 0) {
            if (error) {
                *error = "5250Message: error unmarshalling commands -> " + *error;
            }
            return 0;
        }
        commands.push_back(c);
    }

    return read_bytes;
}

std::vector<uint8_t> Message::marshal(std::string *error) const {
    // The wire length includes its own byte plus flags, reserved, and opcode.
    uint8_t varLen = variableLength;
    if (varLen < 4) {
        varLen = 4;
    }

    // Marshal the commands first so the record length reflects the bytes
    // actually written, not a per-command estimate.
    std::vector<uint8_t> payload;
    for (const auto &cmd : commands) {
        std::string cmdError;
        std::vector<uint8_t> bytes =
            std::visit([&cmdError](const auto &c) { return c.marshal(&cmdError); }, cmd);
        if (bytes.empty()) {
            if (error) {
                *error = "5250Message: error marshalling command";
                if (!cmdError.empty()) {
                    *error += " -> " + cmdError;
                }
            }
            return {};
        }
        payload.insert(payload.end(), bytes.begin(), bytes.end());
    }

    // recordLength is the total record size INCLUDING the 2-byte length
    // field itself — the convention enforced by Decoder::parseData and used
    // on the wire (RFC 1205 logical record length):
    //   len(2) + type(2) + reserved(2) + variable header (varLen) + payload
    const size_t totalLen = 2 + 2 + 2 + static_cast<size_t>(varLen) + payload.size();
    if (totalLen > 0xFFFF) {
        if (error)
            *error = "5250Message: record too large to encode";
        return {};
    }

    std::vector<uint8_t> out;
    out.reserve(totalLen);

    // recordLength (big-endian)
    utils::endianness::be16_write(out, static_cast<uint16_t>(totalLen));

    // recordType (big-endian)
    utils::endianness::be16_write(out, header.recordType.value);
    // fixedReserved (big-endian)
    utils::endianness::be16_write(out, header.fixedReserved);
    // variableLength
    out.push_back(varLen);

    // Variable header content (at least 3 bytes)
    out.push_back(snaFlags);
    out.push_back(variableReserved);
    out.push_back(opcode.value);
    // If caller specified a larger variableLength, pad remaining bytes with zero.
    // Four bytes have already been emitted: length, flags, reserved, and opcode.
    for (uint8_t i = 4; i < varLen; ++i) {
        out.push_back(0x00);
    }

    // Commands / payload
    out.insert(out.end(), payload.begin(), payload.end());

    return out;
}

/**
 * Write a human-readable description to the provided text stream.
 *
 * @param out    Output stream
 * @param indent Indentation level for pretty-printing
 */
void Message::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<5250Message>\n";
    header.describe(out, indent + 1);
    out << indentPrompt << "  │ variableLength : " << static_cast<unsigned>(variableLength) << "\n";
    out << indentPrompt << "  │ snaFlags       : 0x" << utils::hex::to_hex_string_padded_2(snaFlags) << " (" << utils::binary::to_binary_string_padded_8(snaFlags) << ")" << "\n";
    out << indentPrompt << "  │ Reserved       : 0x" << utils::hex::to_hex_string_padded_2(variableReserved) << " (" << utils::binary::to_binary_string_padded_8(variableReserved) << ")" << "\n";
    out << indentPrompt << "  │ Operation Code : 0x" << utils::hex::to_hex_string_padded_2(static_cast<uint8_t>(opcode.value))
        << " (" << opcode.description() << ")\n";
    out << indentPrompt << "  │ commands (" << commands.size() << "): [\n";
    for (const auto &command : commands) {
        std::visit([&](const auto &cmd) { cmd.describe(out, indent + 1); }, command);
    }
    out << indentPrompt << "  │ ]\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message
