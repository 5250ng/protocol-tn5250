// protocol-tn5250 - Standalone TN5250 protocol library
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

#include "ibmrseed.h"

#include <algorithm>
#include <cctype>
#include <random>
#include <openssl/evp.h>
#include <openssl/provider.h>

namespace tn5250::client {

static bool ensureLegacyProvider() {
    static bool loaded = [] {
        OSSL_PROVIDER *legacy = OSSL_PROVIDER_load(nullptr, "legacy");
        OSSL_PROVIDER *deflt = OSSL_PROVIDER_load(nullptr, "default");
        return legacy != nullptr && deflt != nullptr;
    }();
    return loaded;
}

std::vector<uint8_t> IBMRSeed::encryptPassword(const std::string &userId,
                                                 const std::string &password,
                                                 const std::vector<uint8_t> &serverSeed,
                                                 const std::vector<uint8_t> &clientSeed) {
    tn5250::codepage::CodePage cp(tn5250::codepage::CodePage::ID::CP037);
    return encryptPassword(userId, password, serverSeed, clientSeed, cp);
}

std::vector<uint8_t> IBMRSeed::encryptPassword(const std::string &userId,
                                                 const std::string &password,
                                                 const std::vector<uint8_t> &serverSeed,
                                                 const std::vector<uint8_t> &clientSeed,
                                                 const tn5250::codepage::CodePage &cp) {
    if (serverSeed.size() != 8 || clientSeed.size() != 8) return {};

    auto pwEBCDIC = padPasswordEBCDIC(password, cp);
    auto xored = xorWith55(pwEBCDIC);
    auto shifted = leftShift1(xored);
    auto userIdPadded = padUserIdEBCDIC(userId, 8, cp);
    auto pwToken = desEcbEncrypt(shifted, userIdPadded);
    if (pwToken.empty()) return {};

    std::vector<uint8_t> pwseqs(8, 0x00);
    pwseqs[7] = 0x01;

    auto rdrseq = addBigEndian8(serverSeed, pwseqs);

    auto userId16 = padUserIdEBCDIC(userId, 16, cp);
    std::vector<uint8_t> userPart1(userId16.begin(), userId16.begin() + 8);
    std::vector<uint8_t> userPart2(userId16.begin() + 8, userId16.begin() + 16);

    std::vector<uint8_t> xorPart1(8, 0x00);
    std::vector<uint8_t> xorPart2(8, 0x00);
    for (int i = 0; i < 8; ++i) {
        xorPart1[i] = userPart1[i] ^ rdrseq[i];
        xorPart2[i] = userPart2[i] ^ rdrseq[i];
    }

    std::vector<uint8_t> block;
    block.reserve(40);
    block.insert(block.end(), rdrseq.begin(), rdrseq.end());
    block.insert(block.end(), clientSeed.begin(), clientSeed.end());
    block.insert(block.end(), xorPart1.begin(), xorPart1.end());
    block.insert(block.end(), xorPart2.begin(), xorPart2.end());
    block.insert(block.end(), pwseqs.begin(), pwseqs.end());

    std::vector<uint8_t> iv(8, 0x00);
    auto ciphertext = desCbcEncrypt(pwToken, iv, block);
    if (ciphertext.size() < 40) return {};

    return std::vector<uint8_t>(ciphertext.end() - 8, ciphertext.end());
}

std::vector<uint8_t> IBMRSeed::generateClientSeed() {
    std::vector<uint8_t> seed(8);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    for (int i = 0; i < 8; ++i) {
        seed[i] = static_cast<uint8_t>(dist(gen));
    }
    return seed;
}

std::vector<uint8_t> IBMRSeed::escapeNewEnviron(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> escaped;
    escaped.reserve(data.size() * 2);
    for (uint8_t b : data) {
        if (b <= 0x03 || b == 0xFF) {
            escaped.push_back(0x02);
        }
        escaped.push_back(b);
    }
    return escaped;
}

std::vector<uint8_t> IBMRSeed::padPasswordEBCDIC(const std::string &password) {
    tn5250::codepage::CodePage cp(tn5250::codepage::CodePage::ID::CP037);
    return padPasswordEBCDIC(password, cp);
}

std::vector<uint8_t> IBMRSeed::padPasswordEBCDIC(const std::string &password,
                                                   const tn5250::codepage::CodePage &cp) {
    std::string upper;
    upper.reserve(password.size());
    for (char c : password) upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));

    auto ebcdic = cp.fromUnicodeString(upper);
    if (ebcdic.size() > 8) ebcdic.resize(8);
    while (ebcdic.size() < 8) ebcdic.push_back(0x40);
    return ebcdic;
}

std::vector<uint8_t> IBMRSeed::xorWith55(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> result(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ 0x55;
    }
    return result;
}

std::vector<uint8_t> IBMRSeed::leftShift1(const std::vector<uint8_t> &data) {
    if (data.size() != 8) return data;
    std::vector<uint8_t> result(8, 0x00);
    for (int i = 0; i < 7; ++i) {
        result[i] = (data[i] << 1) | (data[i + 1] >> 7);
    }
    result[7] = data[7] << 1;
    return result;
}

std::vector<uint8_t> IBMRSeed::desEcbEncrypt(const std::vector<uint8_t> &key,
                                               const std::vector<uint8_t> &plaintext) {
    if (key.size() != 8 || plaintext.size() != 8) return {};
    ensureLegacyProvider();

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<uint8_t> out(16, 0x00);
    int outLen = 0, finalLen = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_des_ecb(), nullptr, key.data(), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    if (EVP_EncryptUpdate(ctx, out.data(), &outLen, plaintext.data(), 8) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_EncryptFinal_ex(ctx, out.data() + outLen, &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen + finalLen);
    return out;
}

std::vector<uint8_t> IBMRSeed::desCbcEncrypt(const std::vector<uint8_t> &key,
                                               const std::vector<uint8_t> &iv,
                                               const std::vector<uint8_t> &plaintext) {
    if (key.size() != 8 || iv.size() != 8) return {};
    if (plaintext.size() % 8 != 0) return {};
    ensureLegacyProvider();

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<uint8_t> out(plaintext.size() + 16, 0x00);
    int outLen = 0, finalLen = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_des_cbc(), nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    if (EVP_EncryptUpdate(ctx, out.data(), &outLen, plaintext.data(),
                          static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_EncryptFinal_ex(ctx, out.data() + outLen, &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen + finalLen);
    return out;
}

std::vector<uint8_t> IBMRSeed::padUserIdEBCDIC(const std::string &userId, int len) {
    tn5250::codepage::CodePage cp(tn5250::codepage::CodePage::ID::CP037);
    return padUserIdEBCDIC(userId, len, cp);
}

std::vector<uint8_t> IBMRSeed::padUserIdEBCDIC(const std::string &userId, int len,
                                                 const tn5250::codepage::CodePage &cp) {
    std::string upper;
    upper.reserve(userId.size());
    for (char c : userId) upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));

    auto ebcdic = cp.fromUnicodeString(upper);
    if (static_cast<int>(ebcdic.size()) > len) ebcdic.resize(len);
    while (static_cast<int>(ebcdic.size()) < len) ebcdic.push_back(0x40);
    return ebcdic;
}

std::vector<uint8_t> IBMRSeed::addBigEndian8(const std::vector<uint8_t> &a,
                                               const std::vector<uint8_t> &b) {
    if (a.size() != 8 || b.size() != 8) return a;
    std::vector<uint8_t> result(8, 0x00);
    uint16_t carry = 0;
    for (int i = 7; i >= 0; --i) {
        uint16_t sum = a[i] + b[i] + carry;
        result[i] = static_cast<uint8_t>(sum & 0xFF);
        carry = sum >> 8;
    }
    return result;
}

} // namespace tn5250::client
