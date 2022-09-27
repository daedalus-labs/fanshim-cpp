#include <map>
#include <string>
#include <string_view>

inline constexpr std::string_view CONFIGURATION_FILE = "/etc/fanshim.json";

std::map<std::string, int32_t> getConfiguration();
