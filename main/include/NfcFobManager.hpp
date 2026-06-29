#pragma once
#include "config.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <esp_err.h>

class NfcFobManager {
public:
    NfcFobManager();
    ~NfcFobManager() = default;

    /**
     * @brief Initialize the NFC fob manager and load fobs from NVS.
     * @return ESP_OK on success, ESP_ERR_... on failure.
     */
    esp_err_t begin();

    /**
     * @brief Check if NFC fob authentication is enabled.
     * @return true if enabled, false otherwise.
     */
    bool isEnabled() const { return m_config.enabled; }

    /**
     * @brief Enable or disable NFC fob authentication.
     * @param enabled true to enable, false to disable.
     */
    void setEnabled(bool enabled);

    /**
     * @brief Get the current NFC fob configuration.
     * @return Reference to the current configuration.
     */
    const espConfig::nfc_fob_config_t& getConfig() const { return m_config; }

    /**
     * @brief Set the NFC fob configuration and persist to NVS.
     * @param config The new configuration.
     * @return ESP_OK on success.
     */
    esp_err_t setConfig(const espConfig::nfc_fob_config_t& config);

    /**
     * @brief Add a new NFC fob entry.
     * @param uid The UID of the fob (as hex string).
     * @param label Optional label for the fob.
     * @param atqa ATQA of the fob (as hex string, e.g. "0004"). Optional for backward compatibility.
     * @param sak SAK of the fob (as hex string, e.g. "08"). Optional for backward compatibility.
     * @return ESP_OK if added, ESP_ERR_INVALID_STATE if duplicate, ESP_ERR_NO_MEM if full.
     */
    esp_err_t addFob(const std::string& uid, const std::string& label = "", const std::string& atqa = "", const std::string& sak = "");

    /**
     * @brief Remove an NFC fob entry by UID.
     * @param uid The UID of the fob to remove.
     * @return ESP_OK if removed, ESP_ERR_NOT_FOUND if not found.
     */
    esp_err_t removeFob(const std::string& uid);

    /**
     * @brief Check if a given UID is a registered fob.
     * @param uid The UID to check (as hex string).
     * @return true if the UID is registered, false otherwise.
     */
    bool isFobRegistered(const std::string& uid) const;

    /**
     * @brief Check if a given UID with ATQA/SAK matches a registered fob.
     *        If the registered fob has ATQA/SAK set, they must match exactly.
     *        If the registered fob has no ATQA/SAK, only UID is checked (backward compat).
     * @param uid The UID to check (as hex string).
     * @param atqa ATQA of the scanned tag (as hex string).
     * @param sak SAK of the scanned tag (as hex string).
     * @return true if the UID (and optionally ATQA/SAK) matches a registered fob, false otherwise.
     */
    bool isFobRegisteredWithParams(const std::string& uid, const std::string& atqa, const std::string& sak) const;

    /**
     * @brief Get all registered fobs.
     * @return Vector of fob entries.
     */
    std::vector<espConfig::nfc_fob_entry_t> getFobs() const;

    /**
     * @brief Get the number of registered fobs.
     * @return Number of fobs.
     */
    size_t getFobCount() const;

    /**
     * @brief Serialize the current fob configuration to JSON.
     * @return JSON string representation.
     */
    std::string serializeToJson() const;

    /**
     * @brief Deserialize fob configuration from JSON.
     * @param json_string JSON string to parse.
     * @return ESP_OK on success, ESP_FAIL on parse error.
     */
    esp_err_t deserializeFromJson(const std::string& json_string);

private:
    static constexpr const char* NVS_KEY = "NFCFOBDATA";
    static constexpr const char* TAG = "NfcFobManager";
    static constexpr size_t MAX_FOB_ENTRIES = 32;

    espConfig::nfc_fob_config_t m_config;
    mutable std::mutex m_mutex;

    esp_err_t saveToNvs() const;
    esp_err_t loadFromNvs();
};
