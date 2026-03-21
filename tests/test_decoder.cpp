#include <tn5250/client/decoder.h>
#include <cassert>
#include <cstdio>

using namespace tn5250::client;

static std::vector<uint8_t> makeGDS(uint8_t opcode, const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> body;
    body.push_back(0x12);  // GDS type hi
    body.push_back(0xA0);  // GDS type lo
    body.push_back(0x00);  // reserved
    body.push_back(0x00);  // reserved
    body.push_back(0x04);  // varLen = 4
    body.push_back(0x00);  // flagsHi
    body.push_back(0x00);  // flagsLo
    body.push_back(opcode);
    body.insert(body.end(), payload.begin(), payload.end());

    uint16_t recLen = static_cast<uint16_t>(body.size() + 2);
    std::vector<uint8_t> rec;
    rec.push_back(static_cast<uint8_t>((recLen >> 8) & 0xFF));
    rec.push_back(static_cast<uint8_t>(recLen & 0xFF));
    rec.insert(rec.end(), body.begin(), body.end());
    return rec;
}

static std::vector<uint8_t> makeWTDPayload(const std::vector<uint8_t> &displayData) {
    std::vector<uint8_t> p;
    p.push_back(0x04);  // ESC
    p.push_back(0x11);  // Write To Display CC
    p.push_back(0x00);  // ctrl1
    p.push_back(0x00);  // ctrl2
    p.insert(p.end(), displayData.begin(), displayData.end());
    return p;
}

static std::vector<uint8_t> makeClearUnitPayload() {
    return {0x04, 0x40};
}

void testReset() {
    Decoder decoder;
    decoder.parseData({0x05});
    decoder.reset();
    assert(decoder.state() == ParserState::WaitingForCommand);
    std::printf("PASS: testReset\n");
}

void testCommandWithData() {
    std::vector<uint8_t> lastRawData;
    DecoderCallbacks cb;
    cb.onRawScreenData = [&](const std::vector<uint8_t> &data) { lastRawData = data; };

    Decoder decoder(cb);
    std::vector<uint8_t> displayData = {0xC1, 0xC2, 0xC3, 0xC4, 0xC5};
    auto gds = makeGDS(0x02, makeWTDPayload(displayData));
    decoder.parseData(gds);

    assert(decoder.state() == ParserState::WaitingForCommand);
    assert(lastRawData.size() == 5);
    assert(lastRawData[0] == 0xC1);
    std::printf("PASS: testCommandWithData\n");
}

void testClearScreen() {
    int clearCount = 0;
    DecoderCallbacks cb;
    cb.onClearScreen = [&]() { clearCount++; };

    Decoder decoder(cb);
    auto gds = makeGDS(0x02, makeClearUnitPayload());
    decoder.parseData(gds);
    assert(clearCount == 1);
    std::printf("PASS: testClearScreen\n");
}

void testInvalidVarLen() {
    std::string lastError;
    DecoderCallbacks cb;
    cb.onParseError = [&](const std::string &err) { lastError = err; };

    Decoder decoder(cb);
    std::vector<uint8_t> body = {0x12, 0xA0, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x02};
    uint16_t recLen = static_cast<uint16_t>(body.size() + 2);
    std::vector<uint8_t> rec;
    rec.push_back(static_cast<uint8_t>((recLen >> 8) & 0xFF));
    rec.push_back(static_cast<uint8_t>(recLen & 0xFF));
    rec.insert(rec.end(), body.begin(), body.end());

    decoder.parseData(rec);
    assert(!lastError.empty());
    std::printf("PASS: testInvalidVarLen\n");
}

void testStateTransitions() {
    int clearCount = 0;
    DecoderCallbacks cb;
    cb.onClearScreen = [&]() { clearCount++; };

    Decoder decoder(cb);
    auto gds = makeGDS(0x02, makeClearUnitPayload());

    // Send partial
    std::vector<uint8_t> partial(gds.begin(), gds.begin() + 4);
    decoder.parseData(partial);
    assert(clearCount == 0);

    // Send remainder
    std::vector<uint8_t> rest(gds.begin() + 4, gds.end());
    decoder.parseData(rest);
    assert(clearCount == 1);
    std::printf("PASS: testStateTransitions\n");
}

void testMultipleCommands() {
    int clearCount = 0;
    std::vector<uint8_t> lastRawData;
    DecoderCallbacks cb;
    cb.onClearScreen = [&]() { clearCount++; };
    cb.onRawScreenData = [&](const std::vector<uint8_t> &data) { lastRawData = data; };

    Decoder decoder(cb);
    std::vector<uint8_t> displayData = {0xCC, 0xDD};

    auto allData = makeGDS(0x02, makeClearUnitPayload());
    auto gds2 = makeGDS(0x02, makeWTDPayload(displayData));
    allData.insert(allData.end(), gds2.begin(), gds2.end());

    decoder.parseData(allData);
    assert(clearCount == 1);
    assert(lastRawData.size() == 2);
    assert(lastRawData[0] == 0xCC);
    std::printf("PASS: testMultipleCommands\n");
}

int main() {
    testReset();
    testCommandWithData();
    testClearScreen();
    testInvalidVarLen();
    testStateTransitions();
    testMultipleCommands();
    std::printf("All Decoder tests passed.\n");
    return 0;
}
