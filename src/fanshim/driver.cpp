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
inline constexpr std::chrono::milliseconds OVERRIDE_RATE = std::chrono::milliseconds(2000);
inline constexpr std::chrono::milliseconds BUTTON_RATE = std::chrono::milliseconds(500);
inline constexpr std::chrono::milliseconds LED_RATE = std::chrono::milliseconds(150);
inline constexpr double DEFAULT_TEMPERATURE = 25.0;
inline constexpr double S = 1.0;


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
    return ((config.onThreshold() - temperature) / (config.onThreshold() - config.offThreshold())) * NORMALIZATION_FACTOR;
}

Driver::Driver(const Configuration& configuration)
    : _event_loop(uv_default_loop()),
      _sigint_handle(),
      _temp_handle(),
      _override_handle(),
      _button_handle(),
      _led_handle(),
      _config(configuration),
      _tick_count(0),
      _breath_values(),
      _v((configuration.brightness() * 1.0) / MAX_BRIGHTNESS),
      _temp_disabling_button(false),
      _override_disabling_button(false)
{
    uv_signal_init(_event_loop, &_sigint_handle);
    uv_timer_init(_event_loop, &_led_handle);
    uv_timer_init(_event_loop, &_temp_handle);
    uv_timer_init(_event_loop, &_button_handle);
    uv_timer_init(_event_loop, &_override_handle);

    auto tick_callback = std::bind(&Driver::_onTick, this, args::_1);
    Context::instance().setTickCallback(tick_callback);

    auto temp_callback = std::bind(&Driver::_onReadTemperature, this, args::_1);
    Context::instance().setTickCallback(temp_callback);

    auto override_callback = std::bind(&Driver::_onCheckOverride, this, args::_1);
    Context::instance().setOverrideCallback(override_callback);

    auto button_callback = std::bind(&Driver::_onCheckButton, this, args::_1);
    Context::instance().setButtonCallback(button_callback);

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
    uv_timer_stop(&_led_handle);
    uv_timer_stop(&_temp_handle);
    uv_timer_stop(&_button_handle);
    uv_timer_stop(&_override_handle);
    uv_signal_stop(&_sigint_handle);
}

int32_t Driver::run()
{
    uv_signal_cb signal_callback = [](uv_signal_t* handle, int32_t signal_number) {};
    uv_timer_cb tick_callback = [](uv_timer_t* handle) { Context::instance().onTick(handle); };
    uv_timer_cb temp_callback = [](uv_timer_t* handle) { Context::instance().onReadTemperature(handle); };
    uv_timer_cb override_callback = [](uv_timer_t* handle) { Context::instance().onOverrideCheck(handle); };
    uv_timer_cb button_callback = [](uv_timer_t* handle) { Context::instance().onButtonCheck(handle); };

    int32_t result = uv_signal_start(&_sigint_handle, signal_callback, SIGINT);
    if (result) {
        logger().error("Failed to start signal callback: {}", uv_strerror(result));
    }

    result = uv_timer_start(&_temp_handle, temp_callback, 0, _config.delay().count());
    if (result) {
        logger().error("Failed to start temperature check timer: {}", uv_strerror(result));
    }

    result = uv_timer_start(&_override_handle, override_callback, 0, OVERRIDE_RATE.count());
    if (result) {
        logger().error("Failed to start override check timer: {}", uv_strerror(result));
    }

    result = uv_timer_start(&_button_handle, button_callback, 0, BUTTON_RATE.count());
    if (result) {
        logger().error("Failed to start button check timer: {}", uv_strerror(result));
    }

    if (_config.blink() == BlinkType::NO_BLINK) {
        gpio().setBrightness(_config.brightness());
    }
    else {
        logger().debug("Enabling LED type {}", static_cast<uint8_t>(_config.blink()));
        result = uv_timer_start(&_led_handle, tick_callback, 0, LED_RATE.count());
        if (result) {
            logger().error("Failed to start LED timer: {}", uv_strerror(result));
        }
    }

    logger().warn("Fanshim Driver Started");

    return uv_run(_event_loop, UV_RUN_DEFAULT);
}

void Driver::_blinkLED()
{
    if (gpio().getFan()) {
        return;
    }

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

void Driver::_onCheckButton(uv_timer_t* /* unused */)
{
    if (_override_disabling_button || _temp_disabling_button) {
        logger().debug("Driver is skipping the button check due to state [Override: {}, Temperature: {}]", _override_disabling_button, _temp_disabling_button);
        return;
    }

    if (gpio().getButton()) {
        gpio().setFan(true);
    }
    else if (gpio().getFan()) {
        gpio().setFan(false);
    }
}

void Driver::_onCheckOverride(uv_timer_t* /* unused */)
{
    std::error_code ec;
    if (gpio().getFan()) {
        logger().debug("Fan already running, skipping override");
        return;
    }

    if (!std::filesystem::exists(_config.forceFile(), ec)) {
        _override_disabling_button = false;
        return;
    }

    logger().warn("Override file exists, enabling fan");
    _override_disabling_button = true;
    gpio().setFan(true);
}

void Driver::_onReadTemperature(uv_timer_t* /* unused */)
{
    double current_temperature = getCPUTemperature();

    if (current_temperature >= _config.onThreshold()) {
        _temp_disabling_button = true;
        gpio().setFan(true);
    }
    else if (current_temperature < _config.offThreshold()) {
        _temp_disabling_button = false;
        gpio().setFan(false);
    }

    std::ofstream prom_stream(_config.outputFile());
    if (prom_stream.good()) {
        prom_stream << FAN_HEADER << std::to_string(gpio().getFan()) << std::endl;
        prom_stream << TEMP_HEADER << std::to_string(static_cast<int32_t>(current_temperature)) << std::endl;
        prom_stream.close();
    }

    RGB ledColor = hsvToRGB(temperatureToHue(current_temperature, _config), S, _v);
    gpio().setLED(ledColor);

    logger().info("New CPU Temperature: {}, Fan State: {}, LED Color: [0x{:02X}{:02X}{:02X}]", current_temperature, gpio().getFan(), ledColor.red, ledColor.blue, ledColor.green);
}

void Driver::_onSignal(uv_signal_t* /* unused */, int32_t signal)
{
    // Set GPIO to default state
    gpio().setFan(false);
    gpio().setBrightness(OFF);
}

void Driver::_onTick(uv_timer_t* /* unused */)
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
