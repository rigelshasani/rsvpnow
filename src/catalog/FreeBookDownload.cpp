#include "catalog/FreeBookDownload.h"

#include <WiFi.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>

#include <array>
#include <cstdio>

#include "board/BoardStorage.h"
#include "network/VerifiedDownload.h"
#include "network/WifiConnection.h"
#include "storage/fs/StorageFiles.h"
#include "storage/fs/StoragePaths.h"

namespace freebooks {
    namespace {

        struct WifiScope {
            bool owned = WiFi.status() != WL_CONNECTED;
            ~WifiScope() {
                if (owned)
                    net::disconnect();
            }
        };

        bool fileExists(fs::FS& fs, const std::string& path) {
            File file = fs.open(path.c_str(), FILE_READ);
            const bool exists = file && !file.isDirectory();
            file.close();
            return exists;
        }

        // An EPUB is a ZIP whose first entry is the "mimetype" file.
        bool looksLikeEpub(fs::FS& fs, const std::string& path) {
            File file = fs.open(path.c_str(), FILE_READ);
            std::array<uint8_t, 4> magic{};
            const bool ok = file && file.read(magic.data(), magic.size()) == magic.size() && magic[0] == 'P'
                         && magic[1] == 'K' && magic[2] == 3 && magic[3] == 4;
            file.close();
            return ok;
        }

    } // namespace

    std::expected<std::string, std::string> download(const Title& title, const settings::DeviceSettings& settings,
                                                     const settings::DeviceSecrets& secrets,
                                                     const DownloadStatus& status,
                                                     const std::function<bool()>& cancelled) {
        auto& fs = Board::Storage::filesystem();
        if (!Board::Storage::cardSize())
            return std::unexpected("Insert the SD card, then retry");
        if (!StorageFiles::ensureDirectory(StoragePaths::kLibraryPath)
            || !StorageFiles::ensureDirectory(StoragePaths::kBookFilesPath))
            return std::unexpected("Could not open the books folder");

        const std::string name = fileName(title);
        const std::string finalPath = std::string{StoragePaths::kBookFilesPath} + "/" + name;
        // A leading dot keeps the half-written file out of the library scan.
        const std::string stagingPath = std::string{StoragePaths::kBookFilesPath} + "/." + name + ".part";
        if (fileExists(fs, finalPath))
            return finalPath;
        fs.remove(stagingPath.c_str());

        uint64_t total = 0, free = 0;
        if (esp_vfs_fat_info("/sdcard", &total, &free) == ESP_OK && free < uint64_t(title.bytes) * 2 + 1024 * 1024)
            return std::unexpected("SD card is full");

        WifiScope wifi;
        status("Connecting to Wi-Fi", "Middle button cancels", -1);
        if (wifi.owned) {
            const auto connected = net::connectSaved(settings.network, secrets);
            if (!connected)
                return std::unexpected(connected.error() == std::errc::invalid_argument ? "Set up Wi-Fi first"
                                                                                         : "Could not join Wi-Fi; retry");
        }
        if (cancelled())
            return std::unexpected("Cancelled");

        File file = fs.open(stagingPath.c_str(), FILE_WRITE);
        if (!file)
            return std::unexpected("SD card is not writable");

        int lastPercent = -2;
        net::DownloadControl control{cancelled, [&](size_t done, size_t expected) {
            const size_t whole = expected ? expected : title.bytes;
            const int percent = whole ? static_cast<int>(std::min<uint64_t>(100, done * 100ULL / whole)) : -1;
            if (percent == lastPercent)
                return;
            lastPercent = percent;
            char line[48];
            std::snprintf(line, sizeof(line), "%s of %s", sizeLabel(done).c_str(), sizeLabel(whole).c_str());
            status(line, "Middle button cancels", percent);
        }};
        const std::string from = "Downloading from " + std::string{sourceName(title)};
        status(from.c_str(), "Middle button cancels", 0);
        const auto result = net::download(downloadUrl(title), kMaxDownloadBytes, 0,
                                          [&](const uint8_t* data, size_t count) {
                                              return file.write(data, count) == count;
                                          },
                                          control);
        file.flush();
        file.close();
        if (!result) {
            fs.remove(stagingPath.c_str());
            ESP_LOGW("freebooks", "download %u failed: %s", static_cast<unsigned>(title.gutenbergId),
                     result.error().c_str());
            return std::unexpected(result.error());
        }
        if (!looksLikeEpub(fs, stagingPath)) {
            fs.remove(stagingPath.c_str());
            return std::unexpected("Download was not an EPUB; retry later");
        }
        if (!fs.rename(stagingPath.c_str(), finalPath.c_str())) {
            fs.remove(stagingPath.c_str());
            return std::unexpected("Could not save the book");
        }
        ESP_LOGI("freebooks", "saved %s (%u bytes)", finalPath.c_str(), static_cast<unsigned>(*result));
        return finalPath;
    }

} // namespace freebooks
