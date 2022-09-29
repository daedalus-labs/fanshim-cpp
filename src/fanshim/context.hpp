#pragma once

#include <uv.h>

#include <functional>


class Context
{
public:
    using TimerCallback = std::function<void(uv_timer_t*)>;

    static Context& instance()
    {
        static Context instance;
        return instance;
    }

    void onButtonCheck(uv_timer_t* handle)
    {
        return _button_callback(handle);
    }

    void onOverrideCheck(uv_timer_t* handle)
    {
        return _override_callback(handle);
    }

    void onReadTemperature(uv_timer_t* handle)
    {
        return _read_temperature_callback(handle);
    }

    void onTick(uv_timer_t* handle)
    {
        return _tick_callback(handle);
    }

    void setButtonCallback(TimerCallback callback)
    {
        _button_callback = callback;
    }

    void setOverrideCallback(TimerCallback callback)
    {
        _override_callback = callback;
    }

    void setReadTemperatureCallback(TimerCallback callback)
    {
        _read_temperature_callback = callback;
    }

    void setTickCallback(TimerCallback callback)
    {
        _tick_callback = callback;
    }

private:
    TimerCallback _button_callback;
    TimerCallback _override_callback;
    TimerCallback _tick_callback;
    TimerCallback _read_temperature_callback;

    Context() : _button_callback(), _override_callback(), _tick_callback(), _read_temperature_callback()
    {}
};
