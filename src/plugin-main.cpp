/*
Nightbot SR OBS Plugin
Copyright (C) 2025 FabioZumbi12

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QAction>

#include "plugin-support.h"
#include "nightbot-auth.h"
#include "nightbot-api.h"
#include "nightbot-dock.h"
#include "nightbot-settings.h"
#include "SettingsManager.h"
#include <curl/curl.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static void show_settings_dialog(void *private_data)
{
    Q_UNUSED(private_data);
	NightbotSettingsDialog dialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
	dialog.exec();
}

bool obs_module_load(void)
{
	// Inicializa o libcurl globalmente para evitar problemas de heap
	curl_global_init(CURL_GLOBAL_ALL);

	SettingsManager::get().Load();

	NightbotDock *dock_widget = new NightbotDock();
    obs_frontend_add_dock_by_id("nightbot_sr", obs_module_text("Nightbot.DockTitle"), dock_widget);

	// Cria e adiciona a ação no menu "Ferramentas" para abrir as configurações
	obs_frontend_add_tools_menu_item(
		obs_module_text("Nightbot.Settings"), show_settings_dialog, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
    SettingsManager::get().Save();

	// Limpa os recursos do libcurl
	curl_global_cleanup();

	obs_log(LOG_INFO, "plugin unloaded");
}
