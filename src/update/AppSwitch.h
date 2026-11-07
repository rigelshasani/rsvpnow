#pragma once

#include <string>

// Two firmwares can share the board, one per app slot (ota_0 / ota_1 in default_16MB.csv).
// Holding BOOT + PWR for three seconds boots whichever firmware is in the other slot.
namespace AppSwitch {

    constexpr uint32_t kHoldMs = 3000;

    // Name of the firmware in the other slot, or empty when that slot holds no bootable app.
    std::string otherAppName();
    // Makes the other slot boot next and restarts. Returns false (without restarting) if it can't.
    bool bootOtherApp();

} // namespace AppSwitch
