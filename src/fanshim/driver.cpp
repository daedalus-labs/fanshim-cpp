#include "fanshim/driver.hpp"

#include "fanshim/context.hpp"
#include "fanshim/gpio.hpp"
#include "fanshim/logger.hpp"

#include <uv.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <string_view>
#include <thread>

namespace args = std::placeholders;

inline constexpr std::string_view TEMPERATURE_FILE_PATH = "/sys/class/thermal/thermal_zone0/temp";
inline constexpr std::string_view FAN_HEADER = "# HELP cpu_fanshim text file output: fan state.\n# TYPE cpu_fanshim gauge\ncpu_fanshim ";
inline constexpr std::string_view TEMP_HEADER = "# HELP cpu_temp_fanshim text file output: temp.\n# TYPE cpu_temp_fanshim gauge\ncpu_temp_fanshim ";
inline constexpr std::chrono::milliseconds READ_RATE = std::chrono::milliseconds(5000);
inline constexpr std::chrono::milliseconds TICK_RATE = std::chrono::milliseconds(100);
inline constexpr double DEFAULT_TEMPERATURE = 25.0;


static double getCPUTemperature()
{
    std::ifstream stream(TEMPERATURE_FILE_PATH.data(), std::ios::in);
    if (!stream) {
        logger().error("Failed to read CPU Temperature");
        return DEFAULT_TEMPERATURE;
    }

    std::string contents;
    std::getline(stream, contents);
    stream.close();

    try {
        return std::stoi(contents) / 1000.0;
    }
    catch (const std::exception& ex) {
        logger().error("Failed to convert CPU Temperature");
        return DEFAULT_TEMPERATURE;
    }
}

static double hsvk(int32_t n, double hue)
{
    return std::fmod(n + hue / 60.0, 6);
}

static double hsvf(int n, double hue, double s, double v)
{
    double k = hsvk(n, hue);
    return v - v * s * std::max({std::min({k, 4 - k, 1.0}), 0.0});
}

static RGB hsvToRGB(double h, double s, double v)
{
    double hue = h * 360;
    RGB rgb;
    rgb.red = static_cast<uint8_t>(hsvf(5, hue, s, v) * 255);
    rgb.green = static_cast<uint8_t>(hsvf(3, hue, s, v) * 255);
    rgb.blue = static_cast<uint8_t>(hsvf(1, hue, s, v) * 255);
    return rgb;
}

static double temperatureToHue(double temperature, const Configuration& config)
{
    // Hue is expected to be the distance the temperature is from the onThreshold represented as a percentage normalized by 1/3.
    static constexpr double NORMALIZATION_FACTOR = 0.333333;
    if (temperature < config.offThreshold()) {
        return NORMALIZATION_FACTOR;
    }
    else if (temperature > config.onThreshold()) {
        return 0.0;
    }
    else {
        return ((config.onThreshold() - temperature) / (config.onThreshold() - config.offThreshold())) * NORMALIZATION_FACTOR;
    }
}

Driver::Driver(const Configuration& configuration)
    : _event_loop(uv_default_loop()), _sigint_handle(), _temp_handle(), _tick_handle(), _config(configuration), _tick_count(0), _breath_values()
{
    uv_signal_init(_event_loop, &_sigint_handle);
    uv_timer_init(_event_loop, &_tick_handle);
    uv_timer_init(_event_loop, &_temp_handle);

    auto tick_callback = std::bind(&Driver::_onTick, this, args::_1);
    Context::instance().setTickCallback(tick_callback);

    auto temp_callback = std::bind(&Driver::_onReadTemperature, this, args::_1);
    Context::instance().setTickCallback(temp_callback);

    _breath_values.resize(_config.breathBrightness() * 2);
    for (size_t i = 0; i < _config.breathBrightness() * 2; i++) {
        if (i < _config.breathBrightness()) {
            _breath_values[i] = i;
        }
        else {
            _breath_values[i] = 2 * _config.breathBrightness() - i;
        }
    }
}

Driver::~Driver()
{
    // Ensure the GPIO is set back to where it needs to be
    uv_timer_stop(&_tick_handle);
    uv_timer_stop(&_temp_handle);
    uv_signal_stop(&_sigint_handle);
}

int32_t Driver::run()
{
    uv_signal_cb signal_callback = [](uv_signal_t* handle, int32_t signal_number) {};
    uv_timer_cb tick_callback = [](uv_timer_t* handle) { Context::instance().onTick(handle); };
    uv_timer_cb temp_callback = [](uv_timer_t* handle) { Context::instance().onReadTemperature(handle); };

    int32_t result = uv_signal_start(&_sigint_handle, signal_callback, SIGINT);
    if (result) {
        logger().error("Failed to start signal callback: {}", uv_strerror(result));
    }

    result = uv_timer_start(&_tick_handle, tick_callback, 0, TICK_RATE.count());
    if (result) {
        logger().error("Failed to start timer: {}", uv_strerror(result));
    }

    result = uv_timer_start(&_temp_handle, temp_callback, 0, _config.delay().count());
    if (result) {
        logger().error("Failed to start timer: {}", uv_strerror(result));
    }
    logger().warn("Fanshim Driver Started");

    if (_config.blink() == BlinkType::NO_BLINK) {
        gpio().setBrightness(_config.brightness());
    }

    return uv_run(_event_loop, UV_RUN_DEFAULT);
}

void Driver::_blinkLED()
{
    _tick_count++;
    if (_tick_count % 10 == 0) {
        gpio().setBrightness(OFF);
    }
    else if (_tick_count % 5 == 0) {
        gpio().setBrightness(_config.brightness());
    }
}

void Driver::_breatheLED()
{
    gpio().setBrightness(_breath_values[_tick_count % _breath_values.size()]);
    _tick_count++;
}

void Driver::_onReadTemperature(uv_timer_t* handle)
{
    std::error_code ec;
    double current_temperature = getCPUTemperature();

    if (current_temperature >= _config.onThreshold()) {
        gpio().setFan(true);
    }
    else if (current_temperature < _config.offThreshold()) {
        gpio().setFan(false);
    }
    else if (std::filesystem::exists(_config.forceFile(), ec)) {
        gpio().setFan(true);
    }

    std::ofstream prom_stream(_config.outputFile());
    if (prom_stream.good()) {
        prom_stream << FAN_HEADER << std::to_string(gpio().getFan()) << std::endl;
        prom_stream << TEMP_HEADER << std::to_string(static_cast<int32_t>(current_temperature)) << std::endl;
        prom_stream.close();
    }

    logger().info("New CPU Temperature: {}, Fan State: {}", current_temperature, gpio().getFan());
}

void Driver::_onSignal(uv_signal_t* handle, int32_t signal)
{}

void Driver::_onTick(uv_timer_t* handle)
{
    switch (_config.blink()) {
    case BlinkType::BLINK:
        _blinkLED();
    case BlinkType::BREATHE:
        _breatheLED();
    case BlinkType::NO_BLINK:
    default:
        break;
    }
}
