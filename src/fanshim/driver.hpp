#pragma once

#include "fanshim/configuration.hpp"

#include <uv.h>

#include <cstdint>
#include <vector>


class Driver
{
public:
    Driver(const Configuration& configuration);
    ~Driver();

    int32_t run();

private:
    Driver(const Driver&) = delete;
    Driver(Driver&&) = delete;
    Driver& operator=(const Driver&) = delete;
    Driver& operator=(Driver&&) = delete;

    void _blinkLED();
    void _breatheLED();

    void _onCheckButton(uv_timer_t* handle);
    void _onCheckOverride(uv_timer_t* handle);
    void _onReadTemperature(uv_timer_t* handle);
    void _onSignal(uv_signal_t* handle, int32_t signal);
    void _onTick(uv_timer_t* handle);

    uv_loop_t* _event_loop;
    uv_signal_t _sigint_handle;
    uv_timer_t _temp_handle;
    uv_timer_t _override_handle;
    uv_timer_t _button_handle;
    uv_timer_t _led_handle;
    Configuration _config;
    uint8_t _tick_count;
    std::vector<uint8_t> _breath_values;
    double _v;
    bool _temp_disabling_button;
    bool _override_disabling_button;
};