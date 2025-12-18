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

static obs_hotkey_id g_nightbot_resume_hotkey_id;
static obs_hotkey_id g_nightbot_pause_hotkey_id;
static obs_hotkey_id g_nightbot_skip_hotkey_id;

#define HOTKEY_PAUSE_ID "nightbot_sr.pause"
#define HOTKEY_RESUME_ID "nightbot_sr.resume"
#define HOTKEY_SKIP_ID "nightbot_sr.skip"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return get_obs_text("PluginDescription");
}

static void save_hotkeys(obs_data_t *save_data, bool saving, void *private_data)
{
	Q_UNUSED(save_data);
	Q_UNUSED(saving);
	Q_UNUSED(private_data);

	obs_data_array_t *resume_hotkey = obs_hotkey_save(g_nightbot_resume_hotkey_id);
	SettingsManager::get().SetHotkeyData(HOTKEY_RESUME_ID, resume_hotkey);
	obs_data_array_release(resume_hotkey);

	obs_data_array_t *pause_hotkey = obs_hotkey_save(g_nightbot_pause_hotkey_id);
	SettingsManager::get().SetHotkeyData(HOTKEY_PAUSE_ID, pause_hotkey);
	obs_data_array_release(pause_hotkey);

	obs_data_array_t *skip_hotkey = obs_hotkey_save(g_nightbot_skip_hotkey_id);
	SettingsManager::get().SetHotkeyData(HOTKEY_SKIP_ID, skip_hotkey);
	obs_data_array_release(skip_hotkey);

	SettingsManager::get().Save();
}

static void hotkey_pause_song(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	Q_UNUSED(data);
	Q_UNUSED(id);
	Q_UNUSED(hotkey);
	if (pressed) {
		obs_log_info("Pause hotkey pressed");
		NightbotAPI::get().ControlPause();
	}
}

static void hotkey_resume_song(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	Q_UNUSED(data);
	Q_UNUSED(id);
	Q_UNUSED(hotkey);
	if (pressed) {
		obs_log_info("Resume hotkey pressed");
		NightbotAPI::get().ControlPlay();
	}
}

static void hotkey_skip_song(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	Q_UNUSED(data);
	Q_UNUSED(id);
	Q_UNUSED(hotkey);
	if (pressed) {
		obs_log_info("Skip hotkey pressed");
		NightbotAPI::get().ControlSkip();
	}
}

NightbotDock *g_dock_widget = nullptr;

static void show_settings_dialog(void *private_data)
{
    Q_UNUSED(private_data);
	NightbotSettingsDialog dialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
	dialog.exec();
}

const char *obs_module_get_name(void)
{
	return get_obs_text("Nightbot.PluginName");
}

bool obs_module_load(void)
{
	curl_global_init(CURL_GLOBAL_ALL);

	SettingsManager::get().Load();

	g_dock_widget = new NightbotDock();
    obs_frontend_add_dock_by_id("nightbot_sr", get_obs_text("Nightbot.DockTitle"), g_dock_widget);

	obs_frontend_add_tools_menu_item(
		get_obs_text("Nightbot.Settings"), show_settings_dialog, nullptr);

	if (NightbotAuth::get().IsAuthenticated()) {
		NightbotAPI::get().FetchSongQueue(get_obs_text(
			"Nightbot.Queue.PlaylistUser"));
		NightbotAPI::get().FetchSRSettings();
	}

	g_nightbot_resume_hotkey_id = obs_hotkey_register_frontend(
		HOTKEY_RESUME_ID, obs_module_text("Nightbot.Hotkey.Resume"), hotkey_resume_song, nullptr);
	obs_log_info("[Nightbot SR] Hotkey 'Resume' registered with ID: %lu", g_nightbot_resume_hotkey_id);

	g_nightbot_pause_hotkey_id = obs_hotkey_register_frontend(
		HOTKEY_PAUSE_ID, obs_module_text("Nightbot.Hotkey.Pause"), hotkey_pause_song, nullptr);
	obs_log_info("[Nightbot SR] Hotkey 'Pause' registered with ID: %lu", g_nightbot_pause_hotkey_id);

	g_nightbot_skip_hotkey_id = obs_hotkey_register_frontend(
		HOTKEY_SKIP_ID, obs_module_text("Nightbot.Hotkey.Skip"), hotkey_skip_song, nullptr);
	obs_log_info("[Nightbot SR] Hotkey 'Skip' registered with ID: %lu", g_nightbot_skip_hotkey_id);

	obs_frontend_add_save_callback(save_hotkeys, nullptr);

	obs_data_array_t *nightbot_resume_hotkey = SettingsManager::get().GetHotkeyData(HOTKEY_RESUME_ID);
	obs_hotkey_load(g_nightbot_resume_hotkey_id, nightbot_resume_hotkey);
	obs_data_array_release(nightbot_resume_hotkey);

	obs_data_array_t *nightbot_pause_hotkey = SettingsManager::get().GetHotkeyData(HOTKEY_PAUSE_ID);
	obs_hotkey_load(g_nightbot_pause_hotkey_id, nightbot_pause_hotkey);
	obs_data_array_release(nightbot_pause_hotkey);

	obs_data_array_t *nightbot_skip_hotkey = SettingsManager::get().GetHotkeyData(HOTKEY_SKIP_ID);
	obs_hotkey_load(g_nightbot_skip_hotkey_id, nightbot_skip_hotkey);
	obs_data_array_release(nightbot_skip_hotkey);

	obs_log_info("[Nightbot SR] Plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_hotkey_unregister(g_nightbot_resume_hotkey_id);
	obs_hotkey_unregister(g_nightbot_pause_hotkey_id);
	obs_hotkey_unregister(g_nightbot_skip_hotkey_id);

    SettingsManager::get().Save();

	curl_global_cleanup();

	obs_log_info("[Nightbot SR] Plugin unloaded");
}

const char *get_obs_text(const char *key)
{
	return obs_module_text(key);
}

void obs_log_info(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	blogva(LOG_INFO, format, args);
	va_end(args);
}

void obs_log_warning(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	blogva(LOG_WARNING, format, args);
	va_end(args);
}

void obs_log_error(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	blogva(LOG_ERROR, format, args);
	va_end(args);
}
