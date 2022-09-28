#pragma once

#include <cstdint>

inline constexpr uint8_t LOW = 0;
inline constexpr uint8_t HIGH = 1;
inline constexpr uint8_t OFF = 0;

struct RGB
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

class GPIO
{
public:
    static GPIO& control();

    uint8_t getBrightness() const;
    bool getFan() const;
    const RGB& getRGB() const;

    bool setBrightness(uint8_t brightness);
    bool setFan(bool on);
    bool setLED(const RGB& rgb);

private:
    GPIO();
};
