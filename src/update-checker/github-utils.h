#pragma once

#include <functional>
#include <string>

enum {
	OBS_BGREMOVAL_GITHUB_UTILS_SUCCESS = 0,
	OBS_BGREMOVAL_GITHUB_UTILS_ERROR = -1,
};

struct github_utils_release_information {
	int responseCode;
	std::string responseBody;
	std::string version;
};

void github_utils_get_release_information(std::function<void(github_utils_release_information)> callback);
