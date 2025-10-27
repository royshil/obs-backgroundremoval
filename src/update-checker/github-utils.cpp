#include <cstddef>
#include <string>

#include <obs.h>

#include "Client.hpp"
#include "github-utils.h"
#include "plugin-support.h"

static const std::string GITHUB_LATEST_RELEASE_URL =
	"https://api.github.com/repos/occ-ai/obs-backgroundremoval/releases/latest";

void github_utils_get_release_information(std::function<void(github_utils_release_information)> callback)
{
	fetchStringFromUrl(GITHUB_LATEST_RELEASE_URL.c_str(), [callback](std::string responseBody, int code) {
		if (code != 0)
			return;
		// Parse the JSON response
		obs_data_t *data = obs_data_create_from_json(responseBody.c_str());
		if (!data) {
			obs_log(LOG_INFO, "Failed to parse latest release info");
			callback({OBS_BGREMOVAL_GITHUB_UTILS_ERROR, "", ""});
			return;
		}

		// The version is in the "tag_name" property
		std::string version = obs_data_get_string(data, "tag_name");
		std::string body = obs_data_get_string(data, "body");
		obs_data_release(data);

		// remove the "v" prefix in version, if it exists
		if (version[0] == 'v') {
			version = version.substr(1);
		}

		callback({OBS_BGREMOVAL_GITHUB_UTILS_SUCCESS, body, version});
	});
}
