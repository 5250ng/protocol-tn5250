#include <tn5250/client/decoder.h>
#include <cassert>
#include <cstdio>
#include <string>

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

// Regression test for protocol-tn5250#12 / 5250ng#103: a WSF whose data
// contains an embedded 0x04 byte was previously truncated at that byte by the
// scan-for-ESC heuristic. With the length-prefixed walk, the SF body is taken
// from its 2-byte big-endian length and the 0x04 byte is preserved verbatim.
void testWsfLengthPrefixHonoursEmbeddedEsc() {
    std::vector<uint8_t> sfData;
    DecoderCallbacks cb;
    cb.onWriteStructuredField = [&](const std::vector<uint8_t> &d) { sfData = d; };

    Decoder decoder(cb);
    // Single SF: length=0x0008, class=0xD9, type=0x70, body has an embedded 0x04
    std::vector<uint8_t> wsf = {0x04, 0xF3, 0x00, 0x08, 0xD9, 0x70, 0xAA, 0x04, 0xBB};
    auto gds = makeGDS(0x02, wsf);
    decoder.parseData(gds);

    // The callback receives the SF bytes including length and class/type.
    assert(sfData.size() == 8);
    assert(sfData[0] == 0x00 && sfData[1] == 0x08);
    assert(sfData[2] == 0xD9 && sfData[3] == 0x70);
    assert(sfData[6] == 0x04);  // embedded ESC byte kept verbatim
    assert(sfData[7] == 0xBB);
    std::printf("PASS: testWsfLengthPrefixHonoursEmbeddedEsc\n");
}

// Regression test for protocol-tn5250#12 / 5250ng#103: an unknown ESC command
// code used to advance only 2 bytes, leaving the unknown command's payload to
// fall through into the display vector and corrupt the rendered screen via the
// raw-screen-data callback. The decoder must now resync to the next ESC and
// must not emit any raw screen data for the leaked bytes.
void testUnknownEscCommandDoesNotLeakIntoRawData() {
    bool gotRawData = false;
    std::string lastError;
    DecoderCallbacks cb;
    cb.onRawScreenData = [&](const std::vector<uint8_t> &d) {
        if (!d.empty()) gotRawData = true;
    };
    cb.onParseError = [&](const std::string &err) { lastError = err; };

    Decoder decoder(cb);
    // Payload: ESC + unknown CC 0x6E + bogus payload bytes that include 5250
    // order codes (0x11 SBA, 0x03 EA) which the renderer would otherwise treat
    // as orders if they leaked through.
    std::vector<uint8_t> payload = {0x04, 0x6E, 0xD9, 0x36, 0x00, 0x11, 0x03, 0x00};
    auto gds = makeGDS(0x02, payload);
    decoder.parseData(gds);

    assert(!gotRawData);
    assert(lastError.find("unknown ESC command") != std::string::npos);
    std::printf("PASS: testUnknownEscCommandDoesNotLeakIntoRawData\n");
}

// Regression test for protocol-tn5250#12 / 5250ng#103: the per-byte loop used
// to push any non-ESC, non-SOH byte at the outer level into the display vector
// and emit it as raw screen data. Bytes outside any recognised command are now
// reported as parse errors and dropped.
void testStrayBytesAreNotEmittedAsRawScreenData() {
    bool gotRawData = false;
    std::string lastError;
    DecoderCallbacks cb;
    cb.onRawScreenData = [&](const std::vector<uint8_t> &d) {
        if (!d.empty()) gotRawData = true;
    };
    cb.onParseError = [&](const std::string &err) { lastError = err; };

    Decoder decoder(cb);
    // Payload starts with a stray non-ESC, non-SOH byte before any command.
    std::vector<uint8_t> payload = {0xD9, 0x36, 0x04, 0x40};
    auto gds = makeGDS(0x02, payload);
    decoder.parseData(gds);

    assert(!gotRawData);
    assert(lastError.find("stray byte") != std::string::npos);
    std::printf("PASS: testStrayBytesAreNotEmittedAsRawScreenData\n");
}

// STRPCCMD marker detection: a WTD stream that contains the trigger byte 0x80
// followed by the fixed 9-byte PCO signature must fire onStrpccmdRequested
// exactly once and must remove the 10 marker bytes from the rendered display
// data so they do not render as garbage on screen.
void testStrpccmdMarkerFiresCallbackAndIsStrippedFromDisplay() {
    int strpccmdCount = 0;
    std::vector<uint8_t> rawData;
    DecoderCallbacks cb;
    cb.onStrpccmdRequested = [&]() { strpccmdCount++; };
    cb.onRawScreenData = [&](const std::vector<uint8_t> &d) { rawData = d; };

    Decoder decoder(cb);
    // WTD payload: a printable EBCDIC byte, then the 10-byte STRPCCMD marker
    // (trigger + 9-byte signature), then another printable byte. Only the two
    // printable bytes should reach the rendered display; the marker is
    // consumed silently.
    std::vector<uint8_t> displayData = {
        0xC1,
        0x80, 0xFC, 0xD7, 0xC3, 0xD6, 0x40, 0x83, 0x80, 0xA1, 0x80,
        0xC2
    };
    auto gds = makeGDS(0x02, makeWTDPayload(displayData));
    decoder.parseData(gds);

    assert(strpccmdCount == 1);
    assert(rawData.size() == 2);
    assert(rawData[0] == 0xC1);
    assert(rawData[1] == 0xC2);
    std::printf("PASS: testStrpccmdMarkerFiresCallbackAndIsStrippedFromDisplay\n");
}

// A 0x80 byte that is NOT followed by the exact 9-byte PCO signature must be
// treated as an ordinary (non-printable) byte: no callback, no consumption of
// trailing bytes. This guards against accidentally swallowing 9 bytes of
// legitimate data on signature mismatch.
void testStrpccmdMismatchedSignatureDoesNotFire() {
    int strpccmdCount = 0;
    std::vector<uint8_t> rawData;
    DecoderCallbacks cb;
    cb.onStrpccmdRequested = [&]() { strpccmdCount++; };
    cb.onRawScreenData = [&](const std::vector<uint8_t> &d) { rawData = d; };

    Decoder decoder(cb);
    // 0x80 followed by 9 bytes that do NOT match the signature (one byte is
    // wrong). The decoder must not fire the callback, and the trailing bytes
    // must remain in the rendered display data.
    std::vector<uint8_t> displayData = {
        0x80, 0xFC, 0xD7, 0xC3, 0xD6, 0x40, 0x83, 0x80, 0xA1, 0x00,
        0xC2
    };
    auto gds = makeGDS(0x02, makeWTDPayload(displayData));
    decoder.parseData(gds);

    assert(strpccmdCount == 0);
    assert(rawData.size() == 11);
    std::printf("PASS: testStrpccmdMismatchedSignatureDoesNotFire\n");
}

// A truncated marker (trigger byte at end of WTD, fewer than 9 bytes
// remaining) must not crash and must not fire the callback.
void testStrpccmdTruncatedSignatureIsSafe() {
    int strpccmdCount = 0;
    DecoderCallbacks cb;
    cb.onStrpccmdRequested = [&]() { strpccmdCount++; };

    Decoder decoder(cb);
    // 0x80 with only 4 bytes of would-be signature available.
    std::vector<uint8_t> displayData = {0x80, 0xFC, 0xD7, 0xC3, 0xD6};
    auto gds = makeGDS(0x02, makeWTDPayload(displayData));
    decoder.parseData(gds);

    assert(strpccmdCount == 0);
    std::printf("PASS: testStrpccmdTruncatedSignatureIsSafe\n");
}

int main() {
    testReset();
    testCommandWithData();
    testClearScreen();
    testInvalidVarLen();
    testStateTransitions();
    testMultipleCommands();
    testWsfLengthPrefixHonoursEmbeddedEsc();
    testUnknownEscCommandDoesNotLeakIntoRawData();
    testStrayBytesAreNotEmittedAsRawScreenData();
    testStrpccmdMarkerFiresCallbackAndIsStrippedFromDisplay();
    testStrpccmdMismatchedSignatureDoesNotFire();
    testStrpccmdTruncatedSignatureIsSafe();
    std::printf("All Decoder tests passed.\n");
    return 0;
}
