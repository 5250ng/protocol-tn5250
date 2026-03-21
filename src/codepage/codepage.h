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

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tn5250::codepage {

// EBCDIC code page abstraction for multi-language support
// Each code page defines a bidirectional mapping between EBCDIC bytes and Unicode.
class CodePage {
  public:
    enum class ID {
        CP037 = 37,
        CP273 = 273,
        CP277 = 277,
        CP278 = 278,
        CP280 = 280,
        CP284 = 284,
        CP285 = 285,
        CP297 = 297,
        CP500 = 500,
        CP870 = 870,
        CP420 = 420,
        CP424 = 424,
        CP838 = 838,
    };

    explicit CodePage(ID id = ID::CP037);

    ID id() const { return m_id; }
    std::string name() const;

    // Convert EBCDIC byte -> Unicode codepoint
    char32_t toUnicode(uint8_t ebcdic) const;
    // Convert Unicode codepoint -> EBCDIC byte (0x40 if unmappable)
    uint8_t fromUnicode(char32_t ch) const;

    // Bulk conversions
    std::vector<uint8_t> fromUnicodeString(const std::string &utf8) const;

    // Get list of supported code pages
    static std::vector<ID> supportedCodePages();
    static std::string codepageName(ID id);

  private:
    ID m_id;
    std::array<uint16_t, 256> m_toUnicode;
    mutable bool m_reverseBuilt = false;
    mutable std::array<uint8_t, 65536> m_fromUnicodeTable;
    void buildReverseTable() const;

    void initCP037();
    void initCP273();
    void initCP277();
    void initCP278();
    void initCP280();
    void initCP284();
    void initCP285();
    void initCP297();
    void initCP500();
    void initCP870();
    void initCP420();
    void initCP424();
    void initCP838();
};

} // namespace tn5250::codepage
