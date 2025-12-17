#include "SettingsManager.h"
#include <obs-module.h>

static const char *SETTINGS_FILE_NAME = "settings.json";

SettingsManager &SettingsManager::get()
{
	static SettingsManager instance;
	return instance;
}

SettingsManager::~SettingsManager()
{
	if (settings) {
		Save();
		obs_data_release(settings);
	}
}

void SettingsManager::Load()
{
    this->settings = obs_data_create_from_json_file(obs_module_config_path(SETTINGS_FILE_NAME));

	if (!settings) {
        blog(LOG_INFO, "[Nightbot SR/Settings] No config found. Creating new one...");
		settings = obs_data_create();

        // Defaults
        obs_data_set_string(settings, Setting::AccessToken, "");
        obs_data_set_string(settings, Setting::RefreshToken, "");
        obs_data_set_string(settings, Setting::UserName, "");
		obs_data_set_bool(settings, Setting::AutoRefreshEnabled, true);
		obs_data_set_int(settings, Setting::AutoRefreshInterval, 5);
	}
}

void SettingsManager::Save()
{
	if (!settings){
        blog(LOG_ERROR, "[Nightbot SR/Settings] Attempt to save null config.");
		return;
    }

    const char *config_path_c = obs_module_config_path(SETTINGS_FILE_NAME);
	if (!config_path_c) {
		blog(LOG_ERROR, "[Nightbot SR/Settings] Invalid path when saving config.");
		return;
	}

    QString path = QString::fromUtf8(config_path_c);
	QFileInfo info(path);
	QDir dir = info.dir();

	if (!dir.exists())
		dir.mkpath(".");

	if (obs_data_save_json(settings, config_path_c)) {
		blog(LOG_INFO, "[Nightbot SR/Settings] Config saved to: %s", config_path_c);
	} else {
		blog(LOG_WARNING, "[Nightbot SR/Settings] Failed to save config to: %s", config_path_c);
	}
}

void SettingsManager::SetAccessToken(const std::string &token)
{
	obs_data_set_string(settings, Setting::AccessToken, token.c_str());
	Save();
}

std::string SettingsManager::GetAccessToken()
{
	if (!settings)
		return "";

	const char *value = obs_data_get_string(settings, Setting::AccessToken);
	return (value) ? value : "";
}

void SettingsManager::SetRefreshToken(const std::string &token)
{
	obs_data_set_string(settings, Setting::RefreshToken, token.c_str());
	Save();
}

std::string SettingsManager::GetRefreshToken()
{
	if (!settings)
		return "";

	const char *value = obs_data_get_string(settings, Setting::RefreshToken);
	return (value) ? value : "";
}

void SettingsManager::SetUserName(const std::string &name)
{
	obs_data_set_string(settings, Setting::UserName, name.c_str());
	Save();
}

std::string SettingsManager::GetNightUserName()
{
	if (!settings)
		return "";

	const char *value = obs_data_get_string(settings, Setting::UserName);
	return (value) ? value : "";
}

bool SettingsManager::GetAutoRefreshEnabled()
{
	return obs_data_get_bool(settings, Setting::AutoRefreshEnabled);
}

void SettingsManager::SetAutoRefreshInterval(int interval)
{
	obs_data_set_int(settings, Setting::AutoRefreshInterval, interval);
	Save();
}

int SettingsManager::GetAutoRefreshInterval()
{
	return obs_data_get_int(settings, Setting::AutoRefreshInterval);
}

void SettingsManager::SetAutoRefreshEnabled(bool enabled)
{
	obs_data_set_bool(settings, Setting::AutoRefreshEnabled, enabled);
	Save();
}

obs_data_array_t *SettingsManager::GetHotkeyData(const char *key) const
{
	return obs_data_get_array(settings, key);
}

void SettingsManager::SetHotkeyData(const char *key, obs_data_array_t *hotkeyArray)
{
	obs_data_set_array(settings, key, hotkeyArray);
}