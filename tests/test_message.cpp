// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <tn5250/message/message.h>
#include <cassert>
#include <cstdio>

using namespace tn5250::message;

void testMessageRejectsExactlySixByteBuffer() {
    // Regression: a buffer of exactly the fixed-header size (6 bytes)
    // previously passed the header check and then read buffer[6],
    // a one-byte over-read. Must now fail cleanly with an error.
    Message msg;
    std::string err;
    std::vector<uint8_t> buf = {0x12, 0xA0, 0x00, 0x00, 0x00, 0x00};
    uint32_t read = msg.unmarshal(buf, &err);
    assert(read == 0);
    assert(!err.empty());
    std::printf("PASS: testMessageRejectsExactlySixByteBuffer\n");
}

void testMessageAcceptsMinimalVariableHeader() {
    // 6-byte fixed header + 1 variableLength byte (=0, no variable fields)
    Message msg;
    std::string err;
    std::vector<uint8_t> buf = {0x12, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint32_t read = msg.unmarshal(buf, &err);
    assert(read == 7);
    assert(msg.variableLength == 0);
    std::printf("PASS: testMessageAcceptsMinimalVariableHeader\n");
}

// Regression tests for marshal framing (5250ng/5250ng#151): the record must
// contain the marshalled command bytes, and recordLength must equal the
// total record size including the 2-byte length field itself — the
// convention Decoder::parseData frames by.
void testMarshalProducesSelfDescribingLength() {
    Message msg;
    msg.header.recordType = header::RecordType(0x12A0);
    msg.opcode = OperationCode(0x03);

    std::string err;
    std::vector<uint8_t> out = msg.marshal(&err);
    assert(!out.empty());
    const uint16_t recLen = static_cast<uint16_t>((out[0] << 8) | out[1]);
    assert(recLen == out.size());
    // len(2)+type(2)+reserved(2)+varLenByte(1)+varLen(3) = 10
    assert(out.size() == 10);
    std::printf("PASS: testMarshalProducesSelfDescribingLength\n");
}

void testMarshalUnmarshalRoundTripWithCommand() {
    Message msg;
    msg.header.recordType = header::RecordType(0x12A0);
    msg.opcode = OperationCode(0x03);
    command::CommandCsClearScreen clear;
    clear.code = command::CommandCode(command::CommandCode::TN5250_COMMAND_CLEAR_UNIT);
    msg.commands.push_back(clear);

    std::string err;
    std::vector<uint8_t> out = msg.marshal(&err);
    assert(!out.empty());
    // Header (10) + ESC + Clear Unit (2)
    assert(out.size() == 12);
    const uint16_t recLen = static_cast<uint16_t>((out[0] << 8) | out[1]);
    assert(recLen == out.size());
    // The command bytes are actually present
    assert(out[10] == 0x04);
    assert(out[11] == command::CommandCode::TN5250_COMMAND_CLEAR_UNIT);

    Message parsed;
    uint32_t read = parsed.unmarshal(out, &err);
    assert(read == out.size());
    assert(parsed.commands.size() == 1);
    assert(std::holds_alternative<command::CommandCsClearScreen>(parsed.commands.front()));
    std::printf("PASS: testMarshalUnmarshalRoundTripWithCommand\n");
}

void testUnmarshalReadMdtFieldsCommand() {
    command::Command parsed;
    std::string err;
    const std::vector<uint8_t> input = {
        0x04,
        command::CommandCode::TN5250_COMMAND_READ_MDT_FIELDS,
        0x00,
        0x00
    };

    const uint32_t read = command::unmarshalCommand(input, parsed, &err);
    assert(read == input.size());
    assert(err.empty());
    assert(std::holds_alternative<command::CommandRmfReadMdtFields>(parsed));
    std::printf("PASS: testUnmarshalReadMdtFieldsCommand\n");
}

int main() {
    testMessageRejectsExactlySixByteBuffer();
    testMessageAcceptsMinimalVariableHeader();
    testMarshalProducesSelfDescribingLength();
    testMarshalUnmarshalRoundTripWithCommand();
    testUnmarshalReadMdtFieldsCommand();
    std::printf("All Message tests passed.\n");
    return 0;
}
