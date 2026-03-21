#include <tn5250/client/ibmrseed.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>

using tn5250::client::IBMRSeed;

static std::vector<uint8_t> fromHex(const char *hex) {
    std::vector<uint8_t> result;
    for (size_t i = 0; hex[i] && hex[i+1]; i += 2) {
        char buf[3] = {hex[i], hex[i+1], 0};
        result.push_back(static_cast<uint8_t>(std::strtoul(buf, nullptr, 16)));
    }
    return result;
}

static std::string toHex(const std::vector<uint8_t> &data) {
    std::string result;
    for (uint8_t b : data) {
        char buf[3];
        std::snprintf(buf, sizeof(buf), "%02x", b);
        result += buf;
    }
    return result;
}

void testRFC4777Vector() {
    std::string userId = "USER123";
    std::string password = "ABCDEFG";
    auto serverSeed = fromHex("7D4C2319F28004B2");
    auto clientSeed = fromHex("08BEF662D851F4B1");
    std::string expected = "5a58bd50e4dd9b5f";

    auto result = IBMRSeed::encryptPassword(userId, password, serverSeed, clientSeed);
    assert(toHex(result) == expected);
    std::printf("PASS: testRFC4777Vector\n");
}

void testEscapeNewEnviron() {
    std::vector<uint8_t> input = {0x00, 0x01, 0x02, 0x03, 0x04, 0xFF};
    auto escaped = IBMRSeed::escapeNewEnviron(input);
    std::vector<uint8_t> expected = {0x02, 0x00, 0x02, 0x01, 0x02, 0x02, 0x02, 0x03, 0x04, 0x02, 0xFF};
    assert(escaped == expected);
    std::printf("PASS: testEscapeNewEnviron\n");
}

void testGenerateClientSeed() {
    auto seed1 = IBMRSeed::generateClientSeed();
    auto seed2 = IBMRSeed::generateClientSeed();
    assert(seed1.size() == 8);
    assert(seed2.size() == 8);
    assert(seed1 != seed2);
    std::printf("PASS: testGenerateClientSeed\n");
}

int main() {
    testRFC4777Vector();
    testEscapeNewEnviron();
    testGenerateClientSeed();
    std::printf("All IBMRSeed tests passed.\n");
    return 0;
}
