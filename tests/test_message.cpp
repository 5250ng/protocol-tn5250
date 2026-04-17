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

int main() {
    testMessageRejectsExactlySixByteBuffer();
    testMessageAcceptsMinimalVariableHeader();
    std::printf("All Message tests passed.\n");
    return 0;
}
