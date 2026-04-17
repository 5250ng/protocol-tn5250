// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <tn5250/message/command/order/orders/order_sf_start_field.h>
#include <cassert>
#include <cstdio>

using namespace tn5250::message::command::order;

// Build an SF order with the given 16-bit length declaring `dataLen`
// bytes of field data, all set to `fill`.
static std::vector<uint8_t> makeSf(uint16_t length, size_t dataLen, uint8_t fill) {
    std::vector<uint8_t> buf;
    buf.push_back(OrderCode::START_FIELD);
    buf.push_back(0x40); // formatWord1 (bit 6 set — not an FCW)
    buf.push_back(0x00); // formatWord2
    buf.push_back(0x20); // attribute byte (bits 7-5 == 001)
    buf.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(length & 0xFF));
    buf.insert(buf.end(), dataLen, fill);
    return buf;
}

void testSfFieldLengthUnder256() {
    OrderSfStartField sf;
    std::string err;
    auto buf = makeSf(/*length=*/3, /*dataLen=*/3, /*fill=*/0xC1);
    uint32_t read = sf.unmarshal(buf, &err);
    assert(read == buf.size());
    assert(sf.length == 3);
    assert(sf.repeatedCharacter.size() == 3);
    std::printf("PASS: testSfFieldLengthUnder256\n");
}

void testSfFieldLengthOver255ReadsDeclaredCount() {
    // Regression: previously a uint8_t counter wrapped at 256 and the loop
    // consumed the entire remaining buffer. With a uint16_t counter it
    // must read exactly `length` bytes.
    const uint16_t declared = 300;
    OrderSfStartField sf;
    std::string err;
    auto buf = makeSf(/*length=*/declared, /*dataLen=*/declared, /*fill=*/0xC1);
    // Append trailing bytes AFTER the field data; these must not be consumed.
    buf.push_back(0xFF);
    buf.push_back(0xFE);
    uint32_t read = sf.unmarshal(buf, &err);

    // Bytes consumed: 1 (code) + 2 (FW) + 1 (attr) + 2 (len) + declared.
    assert(read == static_cast<uint32_t>(6 + declared));
    assert(sf.length == declared);
    assert(sf.repeatedCharacter.size() == declared);
    std::printf("PASS: testSfFieldLengthOver255ReadsDeclaredCount\n");
}

void testSfFieldLengthTruncatedStopsAtBufferEnd() {
    // When the buffer ends before `length` bytes are available, the loop
    // must stop at the buffer boundary without crashing.
    OrderSfStartField sf;
    std::string err;
    auto buf = makeSf(/*length=*/500, /*dataLen=*/10, /*fill=*/0xC1);
    uint32_t read = sf.unmarshal(buf, &err);
    assert(read == buf.size());
    assert(sf.repeatedCharacter.size() == 10);
    std::printf("PASS: testSfFieldLengthTruncatedStopsAtBufferEnd\n");
}

int main() {
    testSfFieldLengthUnder256();
    testSfFieldLengthOver255ReadsDeclaredCount();
    testSfFieldLengthTruncatedStopsAtBufferEnd();
    std::printf("All SF Order tests passed.\n");
    return 0;
}
