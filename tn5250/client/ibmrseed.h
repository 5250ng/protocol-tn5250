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

#pragma once

#include "tn5250/codepage/codepage.h"
#include <cstdint>
#include <string>
#include <vector>

namespace tn5250::client {

// RFC 4777 IBMRSEED password encryption for TN5250E
class IBMRSeed {
  public:
    static std::vector<uint8_t> encryptPassword(const std::string &userId,
                                                 const std::string &password,
                                                 const std::vector<uint8_t> &serverSeed,
                                                 const std::vector<uint8_t> &clientSeed);

    static std::vector<uint8_t> encryptPassword(const std::string &userId,
                                                 const std::string &password,
                                                 const std::vector<uint8_t> &serverSeed,
                                                 const std::vector<uint8_t> &clientSeed,
                                                 const tn5250::codepage::CodePage &cp);

    static std::vector<uint8_t> generateClientSeed();

    static std::vector<uint8_t> escapeNewEnviron(const std::vector<uint8_t> &data);

  private:
    static std::vector<uint8_t> padPasswordEBCDIC(const std::string &password);
    static std::vector<uint8_t> padPasswordEBCDIC(const std::string &password,
                                                   const tn5250::codepage::CodePage &cp);

    static std::vector<uint8_t> xorWith55(const std::vector<uint8_t> &data);
    static std::vector<uint8_t> leftShift1(const std::vector<uint8_t> &data);

    static std::vector<uint8_t> desEcbEncrypt(const std::vector<uint8_t> &key,
                                               const std::vector<uint8_t> &plaintext);

    static std::vector<uint8_t> desCbcEncrypt(const std::vector<uint8_t> &key,
                                               const std::vector<uint8_t> &iv,
                                               const std::vector<uint8_t> &plaintext);

    static std::vector<uint8_t> padUserIdEBCDIC(const std::string &userId, int len);
    static std::vector<uint8_t> padUserIdEBCDIC(const std::string &userId, int len,
                                                 const tn5250::codepage::CodePage &cp);

    static std::vector<uint8_t> addBigEndian8(const std::vector<uint8_t> &a,
                                               const std::vector<uint8_t> &b);
};

} // namespace tn5250::client
