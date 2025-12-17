#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <string>
#include <obs.h>

#include <QString>
#include <QFileInfo>
#include <QDir>

// Nomes estáticos para as propriedades de configuração
namespace Setting {
	inline const char *AccessToken = "access_token";
	inline const char *RefreshToken = "refresh_token";
	inline const char *UserName = "user_name";
	inline const char *AutoRefreshEnabled = "auto_refresh_enabled";
	inline const char *AutoRefreshInterval = "auto_refresh_interval";
} // namespace Setting

class SettingsManager {
public:
	static SettingsManager &get();
	~SettingsManager();
	void Load();
	void Save();

	void SetAccessToken(const std::string &token);
	std::string GetAccessToken();
	void SetRefreshToken(const std::string &token);
	std::string GetRefreshToken();
	void SetUserName(const std::string &name);
	std::string GetNightUserName();
	void SetAutoRefreshEnabled(bool enabled);
	bool GetAutoRefreshEnabled();
	void SetAutoRefreshInterval(int interval);
	int GetAutoRefreshInterval();

	void SetHotkeyData(const char *key, obs_data_array_t *hotkeyArray);
	obs_data_array_t *GetHotkeyData(const char *key) const;

	// Impede a cópia para garantir o padrão Singleton
	SettingsManager(SettingsManager const &) = delete;
	void operator=(SettingsManager const &) = delete;

private:
	SettingsManager() = default;

	obs_data_t *settings = nullptr;
};

#endif // SETTINGS_MANAGER_H
