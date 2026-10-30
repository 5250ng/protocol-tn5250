#include <tn5250/client/protocol_handler.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using tn5250::client::ProtocolHandler;

static std::vector<uint8_t> fromHex(const char *hex) {
    std::vector<uint8_t> result;
    for (size_t i = 0; hex[i] && hex[i+1]; i += 2) {
        char buf[3] = {hex[i], hex[i+1], 0};
        result.push_back(static_cast<uint8_t>(std::strtoul(buf, nullptr, 16)));
    }
    return result;
}

struct Harness {
    std::vector<std::vector<uint8_t>> received;
    ProtocolHandler handler;

    explicit Harness(bool eor)
        : handler(ProtocolHandler::Config{}, makeCallbacks()) {
        // WILL BINARY, optionally WILL EOR, then SB TERMINAL-TYPE SEND, which
        // completes the handshake.
        handler.onBytesReceived(fromHex("FFFB00"));
        if (eor) handler.onBytesReceived(fromHex("FFFB19"));
        handler.onBytesReceived(fromHex("FFFA1801FFF0"));
        assert(handler.isHandshakeComplete());
    }

    ProtocolHandler::Callbacks makeCallbacks() {
        ProtocolHandler::Callbacks cb;
        cb.onDataReceived = [this](const std::vector<uint8_t> &d) { received.push_back(d); };
        return cb;
    }

    std::vector<uint8_t> all() const {
        std::vector<uint8_t> out;
        for (const auto &d : received) out.insert(out.end(), d.begin(), d.end());
        return out;
    }
};

// Host Integration Server 2000 (KB Q327297) sends X'FF' from the host's GDS
// data without doubling it. IAC followed by a byte below EOR cannot be a
// telnet command, so both bytes must reach the application.
void testUnescapedIacBeforeDataByteIsData() {
    Harness h(false);
    h.handler.onBytesReceived(fromHex("58FF4059"));
    assert(h.all() == fromHex("58FF4059"));
    std::printf("PASS: testUnescapedIacBeforeDataByteIsData\n");
}

// Inside an EOR-framed record an unescaped 0xFF followed by a byte in the
// telnet command range (here 0xF1, EBCDIC '1' and also NOP) is still data.
void testUnescapedIacInsideRecordIsData() {
    Harness h(true);
    h.handler.onBytesReceived(fromHex("000C12A0000004000003FFF1FFEF"));
    assert(h.received.size() == 1);
    assert(h.received[0] == fromHex("000C12A0000004000003FFF1"));
    std::printf("PASS: testUnescapedIacInsideRecordIsData\n");
}

// Records are handed over whole at IAC EOR, even across reads.
void testRecordsDeliveredAtEor() {
    Harness h(true);
    h.handler.onBytesReceived(fromHex("000B12A0"));
    assert(h.received.empty());
    h.handler.onBytesReceived(fromHex("00000400000340FFEF000A12A000000400000AFFEF"));
    assert(h.received.size() == 2);
    assert(h.received[0] == fromHex("000B12A000000400000340"));
    assert(h.received[1] == fromHex("000A12A000000400000A"));
    std::printf("PASS: testRecordsDeliveredAtEor\n");
}

// A GDS length that disagrees with the EOR boundary must not stall the
// decoder: the length is rewritten to the record's actual size.
void testGdsLengthRepairedAtEor() {
    Harness h(true);
    // Header claims 14 bytes, only 12 arrive before EOR.
    h.handler.onBytesReceived(fromHex("000E12A00000040000034142FFEF"));
    assert(h.received.size() == 1);
    assert(h.received[0] == fromHex("000C12A00000040000034142"));

    // Well-formed back-to-back records in one EOR are left alone.
    auto chained = fromHex("000A12A000000400000A000A12A000000400000A");
    assert(ProtocolHandler::repairGdsRecordLength(chained) == chained);
    std::printf("PASS: testGdsLengthRepairedAtEor\n");
}

// Between records, telnet commands such as the NOP keep-alive still work.
void testNopBetweenRecordsIsCommand() {
    Harness h(true);
    h.handler.onBytesReceived(fromHex("FFF1000A12A000000400000AFFEF"));
    assert(h.received.size() == 1);
    assert(h.received[0] == fromHex("000A12A000000400000A"));
    std::printf("PASS: testNopBetweenRecordsIsCommand\n");
}

int main() {
    testUnescapedIacBeforeDataByteIsData();
    testUnescapedIacInsideRecordIsData();
    testRecordsDeliveredAtEor();
    testGdsLengthRepairedAtEor();
    testNopBetweenRecordsIsCommand();
    std::printf("All protocol handler tests passed.\n");
    return 0;
}
