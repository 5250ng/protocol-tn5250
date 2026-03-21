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

#include "devices.h"

#include <algorithm>
#include <cctype>

namespace tn5250::devices {

const std::vector<Device> &supportedDevices() {
    static const std::vector<Device> kDevices = {
        {"IBM-5555-C01", 24, 80, true, true},
        {"IBM-5555-B01", 24, 80, false, true},
        {"IBM-3477-FC", 27, 132, true, false},
        {"IBM-3477-FG", 27, 132, false, false},
        {"IBM-3180-2", 27, 132, false, false},
        {"IBM-3179-2", 24, 80, true, false},
        {"IBM-3196-A1", 24, 80, false, false},
        {"IBM-5292-2", 24, 80, true, false},
        {"IBM-5291-1", 24, 80, false, false},
        {"IBM-5251-11", 24, 80, false, false},
    };
    return kDevices;
}

const Device *findSupportedDevice(const std::string &model) {
    const auto &list = supportedDevices();
    for (const auto &device : list) {
        if (device.model.size() != model.size()) continue;
        bool match = true;
        for (size_t i = 0; i < model.size(); ++i) {
            if (std::toupper(static_cast<unsigned char>(device.model[i])) !=
                std::toupper(static_cast<unsigned char>(model[i]))) {
                match = false;
                break;
            }
        }
        if (match) return &device;
    }
    return nullptr;
}

} // namespace tn5250::devices
