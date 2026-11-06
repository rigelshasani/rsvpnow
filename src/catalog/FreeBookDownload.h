#pragma once

#include <expected>
#include <functional>
#include <string>

#include "catalog/FreeCatalog.h"
#include "settings/SettingsModel.h"

namespace freebooks {

    using DownloadStatus = std::function<void(const char* line1, const char* line2, int percent)>;

    // Downloads one catalog title into /library/books over the saved Wi-Fi.
    // Writes to a hidden staging file and only renames a complete EPUB into place,
    // so an interrupted download never shows up as a broken book. Returns the path.
    std::expected<std::string, std::string> download(const Title& title, const settings::DeviceSettings& settings,
                                                     const settings::DeviceSecrets& secrets,
                                                     const DownloadStatus& status,
                                                     const std::function<bool()>& cancelled);

} // namespace freebooks
