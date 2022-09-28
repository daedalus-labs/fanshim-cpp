#include "fanshim/configuration.hpp"

#include "fanshim/logger.hpp"

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
    // Rules for a "valid" configuration:
    //      1. It must not be empty
    //      2. If it contains On Threshold, it must contain Off Threshold too.
    //          a. On Threshold and Off Threshold must be unsigned integers.
    //          b. On Threshold must be greater than Off Threshold.
    //      3. If it contains Delay, Delay must be an unsigned integer.
    //      4. If it contains Brightness, Brightness must be an unsigned integer.
    //      5. If it contains Blink, Blink must be an unsigned integer.
    //          a. Blink must be a valid BlinkType.
    //      6. If it contains Breath Brightness, Breath Brightness must be an unsigned integer.
    //          a. Breath Brightness must be less than 31
    //      7. If it contains Output File, Output File must be a string.

    if (configuration.empty()) {
        return false;
    }

    if ((configuration.contains(ON_THRESHOLD) && !configuration.contains(OFF_THRESHOLD)) || (!configuration.contains(ON_THRESHOLD) && configuration.contains(OFF_THRESHOLD))) {
        return false;
    }

    if (configuration.contains(ON_THRESHOLD) && configuration.contains(OFF_THRESHOLD)) {
        if (!configuration[ON_THRESHOLD].is_number() || !configuration[OFF_THRESHOLD].is_number() ||
            configuration[ON_THRESHOLD].get<uint8_t>() < configuration[OFF_THRESHOLD].get<uint8_t>()) {
            return false;
        }
    }

    if (configuration.contains(DELAY) && !configuration[DELAY].is_number()) {
        return false;
    }

    if (configuration.contains(BRIGHTNESS) && !configuration[BRIGHTNESS].is_number()) {
        return false;
    }

    if (configuration.contains(BLINK)) {
        if (!configuration[BLINK].is_number() || configuration[BLINK].get<uint8_t>() > static_cast<uint8_t>(BlinkType::BREATHE)) {
            return false;
        }
    }

    if (configuration.contains(BREATH_BRIGHTNESS)) {
        if (!configuration[BREATH_BRIGHTNESS].is_number() || configuration[BLINK].get<uint8_t>() > 31) {
            return false;
        }
    }

    if (configuration.contains(OUTPUT_FILE)) {
        if (!configuration[OUTPUT_FILE].is_string()) {
            return false;
        }
    }

    return true;
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
        logger().error("Configuration file {} is not valid.", configuration_file.native());
        return;
    }

    if (config.contains(ON_THRESHOLD)) {
        _on_threshold = config[ON_THRESHOLD].get<uint8_t>();
        _off_threshold = config[OFF_THRESHOLD].get<uint8_t>();
    }

    if (config.contains(DELAY)) {
        _delay = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(config[DELAY].get<uint8_t>()));
    }

    if (config.contains(BRIGHTNESS)) {
        _brightness = config[BRIGHTNESS].get<uint8_t>();
    }

    if (config.contains(BLINK)) {
        _blink = static_cast<BlinkType>(config[BLINK].get<uint8_t>());
    }

    if (config.contains(BREATH_BRIGHTNESS)) {
        _breath_brightness = config[BREATH_BRIGHTNESS].get<uint8_t>();
    }

    if (config.contains(OUTPUT_FILE)) {
        _output_file = std::filesystem::path(config[OUTPUT_FILE].get<std::string>());
    }
}
