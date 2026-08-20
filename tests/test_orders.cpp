// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <tn5250/message/command/order/orders/order_ra_repeat_to_address.h>
#include <tn5250/message/command/order/orders/order_sba_set_buffer_address.h>
#include <cassert>
#include <cstdio>

using namespace tn5250::message::command::order;

void testRaRejectsThreeByteBuffer() {
    // A 3-byte buffer starting with the RA opcode must not be accepted:
    // the order requires order code + row + column + repeated char = 4 bytes.
    OrderRaRepeatToAddress ra;
    std::string err;
    std::vector<uint8_t> buf = {OrderCode::REPEAT_TO_ADDRESS, 0x01, 0x02};
    uint32_t read = ra.unmarshal(buf, &err);
    assert(read == 0);
    assert(!err.empty());
    std::printf("PASS: testRaRejectsThreeByteBuffer\n");
}

void testRaAcceptsFourByteBuffer() {
    OrderRaRepeatToAddress ra;
    std::string err;
    std::vector<uint8_t> buf = {OrderCode::REPEAT_TO_ADDRESS, 0x01, 0x02, 0x40};
    uint32_t read = ra.unmarshal(buf, &err);
    assert(read == 4);
    assert(ra.rowAddress == 0x01);
    assert(ra.columnAddress == 0x02);
    assert(ra.repeatedCharacter.size() == 1);
    assert(static_cast<uint8_t>(ra.repeatedCharacter[0]) == 0x40);
    std::printf("PASS: testRaAcceptsFourByteBuffer\n");
}

void testSbaAcceptsOrderWithoutDisplayData() {
    OrderSbaSetBufferAddress sba;
    std::string err;
    const std::vector<uint8_t> buf = {
        OrderCode::SET_BUFFER_ADDRESS, 0x01, 0x02
    };

    const uint32_t read = sba.unmarshal(buf, &err);
    assert(read == buf.size());
    assert(err.empty());
    assert(sba.repeatedCharacter.empty());
    std::printf("PASS: testSbaAcceptsOrderWithoutDisplayData\n");
}

void testSbaMarshalPreservesAllDisplayData() {
    OrderSbaSetBufferAddress sba;
    sba.code = OrderCode(OrderCode::SET_BUFFER_ADDRESS);
    sba.rowAddress = 0x01;
    sba.columnAddress = 0x02;
    sba.repeatedCharacter = "ABC";

    const std::vector<uint8_t> out = sba.marshal(nullptr);
    const std::vector<uint8_t> expected = {
        OrderCode::SET_BUFFER_ADDRESS, 0x01, 0x02, 'A', 'B', 'C'
    };
    assert(out == expected);
    std::printf("PASS: testSbaMarshalPreservesAllDisplayData\n");
}

int main() {
    testRaRejectsThreeByteBuffer();
    testRaAcceptsFourByteBuffer();
    testSbaAcceptsOrderWithoutDisplayData();
    testSbaMarshalPreservesAllDisplayData();
    std::printf("All Order tests passed.\n");
    return 0;
}
