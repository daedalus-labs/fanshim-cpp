#include "fanshim/gpio.hpp"

#include "fanshim/logger.hpp"

#include <gpiod.hpp>

#include <chrono>
#include <cstdint>
#include <string_view>
#include <thread>


inline constexpr int32_t CLK_PIN = 14;
inline constexpr int32_t DATA_PIN = 15;
inline constexpr int32_t FAN_PIN = 18;
inline constexpr int32_t BUTTON_PIN = 17;
inline constexpr std::string_view CHIP_NAME = "gpiochip0";
inline constexpr std::string_view CONSUMER_NAME = "fanshim";
inline constexpr std::chrono::microseconds CLOCK_STRETCH = std::chrono::microseconds(5);
inline constexpr size_t NUM_LEDS = 1;


GPIOInterface::GPIOInterface() : _chip(CHIP_NAME.data(), gpiod::chip::OPEN_BY_NAME), _button(), _fan(), _led_clk(), _led_data(), _rgb(), _brightness(OFF)
{
    gpiod::line_request write_request;
    write_request.consumer = CONSUMER_NAME;
    write_request.request_type = gpiod::line_request::DIRECTION_OUTPUT;
    write_request.flags = 0;

    gpiod::line_request read_request;
    read_request.consumer = CONSUMER_NAME;
    read_request.request_type = gpiod::line_request::DIRECTION_INPUT;
    read_request.flags = 0;

    _button = _chip.get_line(BUTTON_PIN);
    _button.request(read_request);

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

GPIOInterface::~GPIOInterface()
{
    _button.release();
    _fan.release();
    _led_clk.release();
    _led_data.release();
}

uint8_t GPIOInterface::getBrightness() const
{
    return _brightness;
}

bool GPIOInterface::getButton() const
{
    return _button.get_value() == HIGH;
}

bool GPIOInterface::getFan() const
{
    return _fan.get_value() == HIGH;
}

const RGB& GPIOInterface::getRGB() const
{
    return _rgb;
}

void GPIOInterface::setBrightness(uint8_t brightness)
{
    if (brightness > MAX_BRIGHTNESS) {
        logger().error("Unable to update brightness, {} is too large", brightness);
        return;
    }

    _brightness = brightness;
    _refreshLED();
}

void GPIOInterface::setFan(bool desired)
{
    if (getFan() == desired) {
        logger().debug("Fan already in desired stated");
        return;
    }

    if (desired) {
        logger().warn("Turning on fan");
        _fan.set_value(HIGH);
    }
    else {
        logger().warn("Turning off fan");
        _fan.set_value(LOW);
    }
}

void GPIOInterface::setLED(const RGB& rgb)
{
    _rgb = rgb;
    _refreshLED();
}

void GPIOInterface::_endFrame()
{
    // An end frame consisting of at least (n/2) bits of 1, where n is the number of LEDs in the string
    // Since this code only controls 1 LED, 1 bits of 1 is all that is necessary.
    for (size_t i = 0; i < NUM_LEDS; ++i) {
        _writeBitToLED(HIGH);
    }
}

void GPIOInterface::_refreshLED()
{
    // Modifying the state of the LED requires writing an entire frame of data.
    // A 32 bit frame for LED data is: [<0xE0+brightness> <blue> <green> <red>]
    // There will also be a start/end value written to notify the driver that values are available.

    _startFrame();

    _writeByteToLED(0xE0 | _brightness);
    _writeByteToLED(_rgb.blue);
    _writeByteToLED(_rgb.green);
    _writeByteToLED(_rgb.red);

    _endFrame();
}

void GPIOInterface::_startFrame()
{
    // Before the LED is modified, a start frame must be written to setup the LED data to come.
    // A start frame is defined as 32 zero bits (<0x00> <0x00> <0x00> <0x00>)
    for (size_t i = 0; i < 32; ++i) {
        _writeBitToLED(LOW);
    }
}

void GPIOInterface::_writeBitToLED(uint8_t value)
{
    _led_data.set_value(value);
    _led_clk.set_value(HIGH);
    std::this_thread::sleep_for(CLOCK_STRETCH);
    _led_clk.set_value(LOW);
    std::this_thread::sleep_for(CLOCK_STRETCH);
}

void GPIOInterface::_writeByteToLED(uint8_t value)
{
    logger().debug("Writing {} to LED pin", value);
    for (uint8_t n = 0; n < __CHAR_BIT__; n++) {
        _writeBitToLED(value & (0x01 << (7 - n)));
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
