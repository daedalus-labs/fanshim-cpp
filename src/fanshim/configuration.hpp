#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

inline constexpr std::string_view DEFAULT_CONFIGURATION_FILE = "/etc/fanshim.json";
inline constexpr std::string_view DEFAULT_PROM_FILE = "/usr/local/etc/node_exp_txt/cpu_fan.prom";
inline constexpr int32_t DEFAULT_ON_THRESHOLD = 60;
inline constexpr int32_t DEFAULT_OFF_THRESHOLD = 50;
inline constexpr std::chrono::milliseconds DEFAULT_DELAY = std::chrono::milliseconds(10000);
inline constexpr uint8_t DEFAULT_BRIGHTNESS = 0;
inline constexpr int32_t DEFAULT_BREATH_BRIGHTNESS = 10;

enum class BlinkType : uint8_t
{
    NO_BLINK = 0,
    BLINK = 1,
    BREATHE = 2
};

struct Configuration
{
public:
    Configuration();
    Configuration(const std::filesystem::path& configuration_file);

    double onThreshold() const;
    double offThreshold() const;
    std::chrono::milliseconds delay() const;
    uint8_t brightness() const;
    BlinkType blink() const;
    uint8_t breathBrightness() const;
    const std::filesystem::path& outputFile() const;

private:
    void _load(const std::filesystem::path& configuration_file);

    double _on_threshold;
    double _off_threshold;
    std::chrono::milliseconds _delay;
    uint8_t _brightness;
    BlinkType _blink;
    uint8_t _breath_brightness;
    std::filesystem::path _output_file;
};
