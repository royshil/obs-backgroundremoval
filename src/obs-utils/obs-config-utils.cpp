#include "obs-config-utils.h"
#include "plugin-support.h"

#include <obs-module.h>
#include <util/config-file.h>
#include <filesystem>
#include <util/platform.h>

void create_config_folder()
{
	char *config_folder_path = obs_module_config_path(NULL);
	if (!config_folder_path) {
		obs_log(LOG_ERROR, "Failed to get config folder path");
		return;
	}
	os_mkdirs(config_folder_path);
	bfree(config_folder_path);
}

int getConfig(config_t **config)
{
	create_config_folder(); // ensure the config folder exists

	// Get the config file
	char *config_file_path = obs_module_config_path("config.ini");

	int ret = config_open(config, config_file_path, CONFIG_OPEN_EXISTING);
	if (ret != CONFIG_SUCCESS) {
		obs_log(LOG_INFO, "Failed to open config file %s", config_file_path);
		bfree(config_file_path);
		return OBS_BGREMOVAL_CONFIG_FAIL;
	}

	bfree(config_file_path);
	return OBS_BGREMOVAL_CONFIG_SUCCESS;
}

int getFlagFromConfig(const char *name, bool *returnValue, bool defaultValue)
{
	// Get the config file
	config_t *config;
	if (getConfig(&config) != OBS_BGREMOVAL_CONFIG_SUCCESS) {
		*returnValue = defaultValue;
		return OBS_BGREMOVAL_CONFIG_FAIL;
	}

	*returnValue = config_get_bool(config, "config", name);
	config_close(config);

	return OBS_BGREMOVAL_CONFIG_SUCCESS;
}

int setFlagInConfig(const char *name, const bool value)
{
	// Get the config file
	config_t *config;
	if (getConfig(&config) != OBS_BGREMOVAL_CONFIG_SUCCESS) {
		return OBS_BGREMOVAL_CONFIG_FAIL;
	}

	config_set_bool(config, "config", name, value);
	config_save(config);
	config_close(config);

	return OBS_BGREMOVAL_CONFIG_SUCCESS;
}
