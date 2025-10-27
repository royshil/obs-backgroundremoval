#include <string>
#include <functional>

void fetchStringFromUrl(const char *urlString, std::function<void(std::string, int)> callback);
