#include "fanshim/gpio.hpp"

#include "fanshim/logger.hpp"

#include <gpiod.hpp>

#include <cstdint>
#include <string_view>


inline constexpr int32_t CLK_PIN = 14;
inline constexpr int32_t DATA_PIN = 15;
inline constexpr int32_t FAN_PIN = 18;
inline constexpr std::string_view CHIP_NAME = "gpiochip0";
inline constexpr std::string_view CONSUMER_NAME = "fanshim";


GPIOInterface::GPIOInterface() : _chip(CHIP_NAME.data(), gpiod::chip::OPEN_BY_NAME), _fan(), _led_clk(), _led_data(), _rgb(), _brightness(OFF)
{
    gpiod::line_request write_request;
    write_request.consumer = CONSUMER_NAME;
    write_request.request_type = gpiod::line_request::DIRECTION_OUTPUT;
    write_request.flags = 0;

    _fan = _chip.get_line(FAN_PIN);
    _fan.request(write_request, 0);

    _led_clk = _chip.get_line(DATA_PIN);
    _led_clk.request(write_request, 0);

    _led_data = _chip.get_line(CLK_PIN);
    _led_data.request(write_request, 0);

    // Previous implementations seem to default to a blueish color, presumably so that if brightness is modified first a color is actually present.
    _rgb.red = 0;
    _rgb.green = 0;
    _rgb.blue = 190;
}

uint8_t GPIOInterface::getBrightness() const
{
    return _brightness;
}

bool GPIOInterface::getFan() const
{
    return _fan.get_value() == HIGH;
}

const RGB& GPIOInterface::getRGB() const
{
    return _rgb;
}

bool GPIOInterface::setBrightness(uint8_t brightness)
{
    if (brightness > MAX_BRIGHTNESS) {
        logger().error("Unable to update brightness, {} is too large", brightness);
        return;
    }

    _brightness = brightness;
    _refreshLED();
}

bool GPIOInterface::setFan(bool desired)
{
    if (getFan() == desired) {
        logger().debug("Fan already in desired stated");
        return;
    }

    if (desired) {
        logger().info("Turning on fan");
        _fan.set_value(HIGH);
    }
    else {
        logger().info("Turning off fan");
        _fan.set_value(LOW);
    }
}

bool GPIOInterface::setLED(const RGB& rgb)
{
    _rgb = rgb;
    _refreshLED();
}

void GPIOInterface::_endFrame()
{
    // An end frame consisting of at least (n/2) bits of 1, where n is the number of LEDs in the string
    // Since this code only controls 1 LED, 1 bits of 1 is all that is necessary.
    _led_data.set_value(HIGH);
    for (int i = 0; i < 1; ++i) {
        _led_clk.set_value(HIGH);
        _led_clk.set_value(LOW);
    }
}

void GPIOInterface::_refreshLED()
{
    // Modifying the state of the LED requires writing an entire "frame."
    // A 32 bit LED frame for each LED in the string (<0xE0+brightness> <blue> <green> <red>)
    // There will also be a start/end value written to notify the driver that values are available.

    _startFrame();

    _writeByteToLED(0b11100000 | _brightness);
    _writeByteToLED(_rgb.blue);
    _writeByteToLED(_rgb.green);
    _writeByteToLED(_rgb.red);

    _endFrame();
}

void GPIOInterface::_startFrame()
{
    _led_data.set_value(LOW);
    for (int i = 0; i < 32; ++i) {
        _led_clk.set_value(HIGH);
        _led_clk.set_value(LOW);
    }
}

void GPIOInterface::_writeByteToLED(uint8_t value)
{
    logger().debug("Writing {} to LED pin", value);
    for (uint8_t n = 0; n < 8; n++) {
        _led_data.set_value(value & (0x01 << (7 - n)));
        _led_clk.set_value(HIGH);
        _led_clk.set_value(LOW);
    }
}

GPIOContext& GPIOContext::instance()
{
    static GPIOContext instance;
    return instance;
}

GPIOInterface& GPIOContext::gpio()
{
    return _gpio;
}

GPIOContext::GPIOContext() : _gpio()
{}

GPIOContext::~GPIOContext() = default;
