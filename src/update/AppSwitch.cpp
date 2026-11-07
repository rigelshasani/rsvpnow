#include "update/AppSwitch.h"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

namespace AppSwitch {
    namespace {
        const esp_partition_t* otherSlot(esp_app_desc_t* description) {
            const esp_partition_t* running = esp_ota_get_running_partition();
            const esp_partition_t* other = esp_ota_get_next_update_partition(running);
            if (other == nullptr || other == running)
                return nullptr;
            esp_app_desc_t ignored;
            return esp_ota_get_partition_description(other, description ? description : &ignored) == ESP_OK ? other
                                                                                                             : nullptr;
        }
    } // namespace

    std::string otherAppName() {
        esp_app_desc_t description{};
        if (otherSlot(&description) == nullptr)
            return {};
        return description.project_name[0] ? std::string{description.project_name} : std::string{"other app"};
    }

    bool bootOtherApp() {
        const esp_partition_t* other = otherSlot(nullptr);
        // Verifies the whole image before changing which slot boots.
        if (other == nullptr || esp_ota_set_boot_partition(other) != ESP_OK) {
            ESP_LOGW("appswitch", "no bootable firmware in the other slot");
            return false;
        }
        ESP_LOGI("appswitch", "booting %s", other->label);
        esp_restart();
        return true;
    }

} // namespace AppSwitch
