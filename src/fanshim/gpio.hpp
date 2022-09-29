#pragma once

#include <gpiod.hpp>

#include <cstdint>


inline constexpr uint8_t LOW = 0;
inline constexpr uint8_t HIGH = 1;
inline constexpr uint8_t OFF = 0;
inline constexpr uint8_t MAX_BRIGHTNESS = 31;

struct RGB
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

class GPIOInterface
{
public:
    GPIOInterface();
    ~GPIOInterface();

    uint8_t getBrightness() const;
    bool getButton() const;
    bool getFan() const;
    const RGB& getRGB() const;

    void setBrightness(uint8_t brightness);
    void setFan(bool desired);
    void setLED(const RGB& rgb);

private:
    void _endFrame();
    void _refreshLED();
    void _startFrame();
    void _writeBitToLED(uint8_t value);
    void _writeByteToLED(uint8_t value);

    gpiod::chip _chip;
    gpiod::line _button;
    gpiod::line _fan;
    gpiod::line _led_clk;
    gpiod::line _led_data;
    RGB _rgb;
    uint8_t _brightness;
};

class GPIOContext
{
public:
    static GPIOContext& instance();

    GPIOInterface& gpio();

private:
    GPIOContext();
    ~GPIOContext();

    GPIOInterface _gpio;
};

inline GPIOInterface& gpio()
{
    return GPIOContext::instance().gpio();
}
