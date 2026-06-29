#include "NfcFobManager.hpp"
#include "cJSON.h"
#include "nvs.h"
#include "esp_log.h"
#include "fmt/format.h"
#include <cstring>
#include <algorithm>

NfcFobManager::NfcFobManager() {}

esp_err_t NfcFobManager::begin() {
    return loadFromNvs();
}

void NfcFobManager::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.enabled = enabled;
    saveToNvs();
}

esp_err_t NfcFobManager::setConfig(const espConfig::nfc_fob_config_t& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    return saveToNvs();
}

esp_err_t NfcFobManager::addFob(const std::string& uid, const std::string& label, const std::string& atqa, const std::string& sak) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Normalize UID to uppercase
    std::string normalizedUid = uid;
    std::transform(normalizedUid.begin(), normalizedUid.end(), normalizedUid.begin(), ::toupper);

    // Normalize ATQA to uppercase
    std::string normalizedAtqa = atqa;
    std::transform(normalizedAtqa.begin(), normalizedAtqa.end(), normalizedAtqa.begin(), ::toupper);

    // Normalize SAK to uppercase
    std::string normalizedSak = sak;
    std::transform(normalizedSak.begin(), normalizedSak.end(), normalizedSak.begin(), ::toupper);

    // Check for duplicate
    for (const auto& fob : m_config.fobs) {
        std::string fobUid = fob.uid;
        std::transform(fobUid.begin(), fobUid.end(), fobUid.begin(), ::toupper);
        if (fobUid == normalizedUid) {
            ESP_LOGW(TAG, "Fob with UID %s already exists", normalizedUid.c_str());
            return ESP_ERR_INVALID_STATE;
        }
    }

    // Check max entries
    if (m_config.fobs.size() >= MAX_FOB_ENTRIES) {
        ESP_LOGE(TAG, "Maximum fob entries (%u) reached", MAX_FOB_ENTRIES);
        return ESP_ERR_NO_MEM;
    }

    espConfig::nfc_fob_entry_t entry;
    entry.uid = normalizedUid;
    entry.label = label;
    entry.atqa = normalizedAtqa;
    entry.sak = normalizedSak;
    m_config.fobs.push_back(entry);

    esp_err_t err = saveToNvs();
    if (err == ESP_OK) {
        if (!label.empty() || !atqa.empty()) {
            ESP_LOGI(TAG, "Fob added: UID=%s, label=%s, atqa=%s, sak=%s", normalizedUid.c_str(), label.c_str(), atqa.c_str(), sak.c_str());
        } else {
            ESP_LOGI(TAG, "Fob added: UID=%s", normalizedUid.c_str());
        }
    }
    return err;
}

esp_err_t NfcFobManager::removeFob(const std::string& uid) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string normalizedUid = uid;
    std::transform(normalizedUid.begin(), normalizedUid.end(), normalizedUid.begin(), ::toupper);

    auto it = std::find_if(m_config.fobs.begin(), m_config.fobs.end(),
        [&normalizedUid](const espConfig::nfc_fob_entry_t& fob) {
            std::string fobUid = fob.uid;
            std::transform(fobUid.begin(), fobUid.end(), fobUid.begin(), ::toupper);
            return fobUid == normalizedUid;
        });

    if (it == m_config.fobs.end()) {
        ESP_LOGW(TAG, "Fob with UID %s not found", normalizedUid.c_str());
        return ESP_ERR_NOT_FOUND;
    }

    m_config.fobs.erase(it);
    return saveToNvs();
}

bool NfcFobManager::isFobRegistered(const std::string& uid) const {
    std::string normalizedUid = uid;
    std::transform(normalizedUid.begin(), normalizedUid.end(), normalizedUid.begin(), ::toupper);

    for (const auto& fob : m_config.fobs) {
        std::string fobUid = fob.uid;
        std::transform(fobUid.begin(), fobUid.end(), fobUid.begin(), ::toupper);
        if (fobUid == normalizedUid) {
            return true;
        }
    }
    return false;
}

bool NfcFobManager::isFobRegisteredWithParams(const std::string& uid, const std::string& atqa, const std::string& sak) const {
    std::string normalizedUid = uid;
    std::transform(normalizedUid.begin(), normalizedUid.end(), normalizedUid.begin(), ::toupper);
    std::string normalizedAtqa = atqa;
    std::transform(normalizedAtqa.begin(), normalizedAtqa.end(), normalizedAtqa.begin(), ::toupper);
    std::string normalizedSak = sak;
    std::transform(normalizedSak.begin(), normalizedSak.end(), normalizedSak.begin(), ::toupper);

    for (const auto& fob : m_config.fobs) {
        std::string fobUid = fob.uid;
        std::transform(fobUid.begin(), fobUid.end(), fobUid.begin(), ::toupper);
        if (fobUid != normalizedUid) {
            continue;
        }

        // If the registered fob has ATQA/SAK set, they must match exactly
        if (!fob.atqa.empty() && !fob.sak.empty()) {
            std::string fobAtqa = fob.atqa;
            std::string fobSak = fob.sak;
            std::transform(fobAtqa.begin(), fobAtqa.end(), fobAtqa.begin(), ::toupper);
            std::transform(fobSak.begin(), fobSak.end(), fobSak.begin(), ::toupper);
            if (fobAtqa == normalizedAtqa && fobSak == normalizedSak) {
                return true;
            }
        } else {
            // No ATQA/SAK stored — backward compatibility, only UID match
            return true;
        }
    }
    return false;
}

std::vector<espConfig::nfc_fob_entry_t> NfcFobManager::getFobs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.fobs;
}

size_t NfcFobManager::getFobCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.fobs.size();
}

std::string NfcFobManager::serializeToJson() const {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "enabled", m_config.enabled);

    cJSON *fobsArray = cJSON_CreateArray();
    for (const auto& fob : m_config.fobs) {
        cJSON *fobObj = cJSON_CreateObject();
        cJSON_AddStringToObject(fobObj, "uid", fob.uid.c_str());
        cJSON_AddStringToObject(fobObj, "label", fob.label.c_str());
        if (!fob.atqa.empty()) {
            cJSON_AddStringToObject(fobObj, "atqa", fob.atqa.c_str());
        }
        if (!fob.sak.empty()) {
            cJSON_AddStringToObject(fobObj, "sak", fob.sak.c_str());
        }
        cJSON_AddItemToArray(fobsArray, fobObj);
    }
    cJSON_AddItemToObject(root, "fobs", fobsArray);

    char* json = cJSON_PrintUnformatted(root);
    std::string result(json ? json : "");
    free(json);
    cJSON_Delete(root);
    return result;
}

esp_err_t NfcFobManager::deserializeFromJson(const std::string& json_string) {
    cJSON *root = cJSON_Parse(json_string.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    cJSON *enabled = cJSON_GetObjectItem(root, "enabled");
    if (enabled && cJSON_IsBool(enabled)) {
        m_config.enabled = cJSON_IsTrue(enabled);
    }

    cJSON *fobsArray = cJSON_GetObjectItem(root, "fobs");
    if (fobsArray && cJSON_IsArray(fobsArray)) {
        m_config.fobs.clear();
        for (cJSON *item = fobsArray->child; item != nullptr; item = item->next) {
            if (cJSON_IsObject(item)) {
                cJSON *uid = cJSON_GetObjectItem(item, "uid");
                cJSON *label = cJSON_GetObjectItem(item, "label");
                cJSON *atqa = cJSON_GetObjectItem(item, "atqa");
                cJSON *sak = cJSON_GetObjectItem(item, "sak");
                if (uid && cJSON_IsString(uid)) {
                    espConfig::nfc_fob_entry_t fobEntry;
                    fobEntry.uid = uid->valuestring;
                    std::transform(fobEntry.uid.begin(), fobEntry.uid.end(), fobEntry.uid.begin(), ::toupper);
                    if (label && cJSON_IsString(label)) {
                        fobEntry.label = label->valuestring;
                    }
                    if (atqa && cJSON_IsString(atqa)) {
                        fobEntry.atqa = atqa->valuestring;
                        std::transform(fobEntry.atqa.begin(), fobEntry.atqa.end(), fobEntry.atqa.begin(), ::toupper);
                    }
                    if (sak && cJSON_IsString(sak)) {
                        fobEntry.sak = sak->valuestring;
                        std::transform(fobEntry.sak.begin(), fobEntry.sak.end(), fobEntry.sak.begin(), ::toupper);
                    }
                    m_config.fobs.push_back(fobEntry);
                }
            }
        }
    }

    cJSON_Delete(root);
    return saveToNvs();
}

esp_err_t NfcFobManager::saveToNvs() const {
    std::string json = serializeToJson();

    nvs_handle handle;
    esp_err_t err = nvs_open("SAVED_DATA", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, NVS_KEY, json.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Fob config saved to NVS (%zu fobs)", m_config.fobs.size());
    }
    return err;
}

esp_err_t NfcFobManager::loadFromNvs() {
    nvs_handle handle;
    esp_err_t err = nvs_open("SAVED_DATA", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    size_t len = 0;
    err = nvs_get_str(handle, NVS_KEY, nullptr, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved fob config found, using defaults");
        nvs_close(handle);
        return ESP_OK;
    }

    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    std::string json(len, '\0');
    err = nvs_get_str(handle, NVS_KEY, &json[0], &len);
    nvs_close(handle);

    if (err == ESP_OK && !json.empty()) {
        ESP_LOGI(TAG, "Loaded fob config from NVS (%zu bytes)", len);
        // Parse and merge with defaults
        cJSON *root = cJSON_Parse(json.c_str());
        if (root) {
            cJSON *enabled = cJSON_GetObjectItem(root, "enabled");
            if (enabled && cJSON_IsBool(enabled)) {
                m_config.enabled = cJSON_IsTrue(enabled);
            }

            cJSON *fobsArray = cJSON_GetObjectItem(root, "fobs");
            if (fobsArray && cJSON_IsArray(fobsArray)) {
                for (cJSON *item = fobsArray->child; item != nullptr; item = item->next) {
                    if (cJSON_IsObject(item)) {
                        cJSON *uid = cJSON_GetObjectItem(item, "uid");
                        cJSON *label = cJSON_GetObjectItem(item, "label");
                        cJSON *atqa = cJSON_GetObjectItem(item, "atqa");
                        cJSON *sak = cJSON_GetObjectItem(item, "sak");
                        if (uid && cJSON_IsString(uid)) {
                            espConfig::nfc_fob_entry_t fobEntry;
                            fobEntry.uid = uid->valuestring;
                            std::transform(fobEntry.uid.begin(), fobEntry.uid.end(), fobEntry.uid.begin(), ::toupper);
                            if (label && cJSON_IsString(label)) {
                                fobEntry.label = label->valuestring;
                            }
                            if (atqa && cJSON_IsString(atqa)) {
                                fobEntry.atqa = atqa->valuestring;
                                std::transform(fobEntry.atqa.begin(), fobEntry.atqa.end(), fobEntry.atqa.begin(), ::toupper);
                            }
                            if (sak && cJSON_IsString(sak)) {
                                fobEntry.sak = sak->valuestring;
                                std::transform(fobEntry.sak.begin(), fobEntry.sak.end(), fobEntry.sak.begin(), ::toupper);
                            }
                            m_config.fobs.push_back(fobEntry);
                        }
                    }
                }
            }
            ESP_LOGI(TAG, "Loaded %zu fobs from NVS", m_config.fobs.size());
            cJSON_Delete(root);
        }
    }

    return ESP_OK;
}
