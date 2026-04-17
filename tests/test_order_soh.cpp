// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <tn5250/message/command/order/orders/order_soh_start_of_header.h>
#include <cassert>
#include <cstdio>

using namespace tn5250::message::command::order;

void testSohRejectsShortBuffer() {
    // Buffers shorter than the full 9-byte SOH order must be rejected.
    // Previously only buffer.size() < 3 was checked, but the parser reads
    // up to buffer[8] unconditionally.
    for (size_t sz = 3; sz < 9; ++sz) {
        OrderSohStartOfHeader soh;
        std::string err;
        std::vector<uint8_t> buf(sz, 0x00);
        buf[0] = OrderCode::START_OF_HEADER;
        uint32_t read = soh.unmarshal(buf, &err);
        assert(read == 0);
        assert(!err.empty());
    }
    std::printf("PASS: testSohRejectsShortBuffer\n");
}

void testSohAcceptsNineByteBuffer() {
    OrderSohStartOfHeader soh;
    std::string err;
    std::vector<uint8_t> buf = {
        OrderCode::START_OF_HEADER,
        0x07,        // length
        0x00,        // flags
        0x00,        // flagsReserved
        0x05,        // resequenceToField
        0x18,        // errorRow
        0xAA, 0xBB, 0xCC  // commandKeySwitch1..3
    };
    uint32_t read = soh.unmarshal(buf, &err);
    assert(read == 9);
    assert(soh.length == 0x07);
    assert(soh.resequenceToField == 0x05);
    assert(soh.errorRow == 0x18);
    assert(soh.commandKeySwitch1 == 0xAA);
    assert(soh.commandKeySwitch2 == 0xBB);
    assert(soh.commandKeySwitch3 == 0xCC);
    std::printf("PASS: testSohAcceptsNineByteBuffer\n");
}

int main() {
    testSohRejectsShortBuffer();
    testSohAcceptsNineByteBuffer();
    std::printf("All SOH Order tests passed.\n");
    return 0;
}
