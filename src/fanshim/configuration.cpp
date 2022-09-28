#include "fanshim/configuration.hpp"

#include <nlohmann/json.hpp>

#include <cerrno>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

using json = nlohmann::json;

inline constexpr std::string_view ON_THRESHOLD = "on-threshold";
inline constexpr std::string_view OFF_THRESHOLD = "off-threshold";
inline constexpr std::string_view DELAY = "delay";
inline constexpr std::string_view BRIGHTNESS = "brightness";
inline constexpr std::string_view BLINK = "blink";
inline constexpr std::string_view BREATH_BRIGHTNESS = "breath-brightness";
inline constexpr std::string_view OUTPUT_FILE = "output-file";


static bool isValid(const json& configuration)
{
    // Optional fields?
    return !configuration.empty() && configuration[ON_THRESHOLD].is_number() && configuration[OFF_THRESHOLD].is_number() && configuration[DELAY].is_number() &&
           configuration[BRIGHTNESS].is_number() && configuration[BLINK].is_number() && configuration[BREATH_BRIGHTNESS].is_number() &&
           configuration[ON_THRESHOLD].get<int32_t>() >= configuration[OFF_THRESHOLD].get<int32_t>() && configuration[DELAY].get<uint32_t>() > 0 &&
           configuration[BRIGHTNESS].get<int32_t>() > 0 && configuration[BLINK].get<int32_t>() > 0 && configuration[BLINK].get<int32_t>() < 3 &&
           configuration[BREATH_BRIGHTNESS].get<int32_t>() > 0 && configuration[BREATH_BRIGHTNESS].get<int32_t>() <= 31;
}

Configuration::Configuration() : Configuration(std::filesystem::path(DEFAULT_CONFIGURATION_FILE))
{}

Configuration::Configuration(const std::filesystem::path& configuration_file)
    : _on_threshold(DEFAULT_ON_THRESHOLD),
      _off_threshold(DEFAULT_OFF_THRESHOLD),
      _delay(DEFAULT_DELAY),
      _brightness(DEFAULT_BRIGHTNESS),
      _blink(BlinkType::NO_BLINK),
      _breath_brightness(DEFAULT_BREATH_BRIGHTNESS),
      _output_file(DEFAULT_PROM_FILE)
{
    _load(configuration_file);
}

double Configuration::onThreshold() const
{
    return _on_threshold;
}

double Configuration::offThreshold() const
{
    return _off_threshold;
}

std::chrono::milliseconds Configuration::delay() const
{
    return _delay;
}

uint8_t Configuration::brightness() const
{
    return _brightness;
}

BlinkType Configuration::blink() const
{
    return _blink;
}

uint8_t Configuration::breathBrightness() const
{
    return _breath_brightness;
}

const std::filesystem::path& Configuration::outputFile() const
{
    return _output_file;
}

void Configuration::_load(const std::filesystem::path& configuration_file)
{
    json config;

    std::ifstream config_stream(configuration_file, std::ios::in);
    if (!config_stream) {
        return;
    }

    config = json::parse(config_stream);
    config_stream.close();

    if (!isValid(config)) {
        return;
    }

    _on_threshold = config[ON_THRESHOLD].get<int32_t>();
    _off_threshold = config[OFF_THRESHOLD].get<int32_t>();
    _delay = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(config[DELAY].get<uint32_t>()));
    _brightness = config[BRIGHTNESS].get<uint8_t>();
    _blink = static_cast<BlinkType>(config[BLINK].get<uint8_t>());
    _breath_brightness = config[BREATH_BRIGHTNESS].get<uint8_t>();

    if (config[OUTPUT_FILE].is_string()) {
        _output_file = std::filesystem::path(config[OUTPUT_FILE].get<std::string>());
    }
}
