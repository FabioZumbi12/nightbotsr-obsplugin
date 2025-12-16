#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <string>
#include <obs.h>

#include <QString>
#include <QFileInfo>
#include <QDir>

// Nomes estáticos para as propriedades de configuração
namespace Setting {
	static const char *AccessToken = "access_token";
	static const char *RefreshToken = "refresh_token";
	static const char *UserName = "user_name";
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

	// Impede a cópia para garantir o padrão Singleton
	SettingsManager(SettingsManager const &) = delete;
	void operator=(SettingsManager const &) = delete;

private:
	SettingsManager() = default;

	obs_data_t *settings = nullptr;
};

#endif // SETTINGS_MANAGER_H