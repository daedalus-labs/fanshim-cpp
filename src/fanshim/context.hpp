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

    void onReadTemperature(uv_timer_t* handle)
    {
        return _read_temperature_callback(handle);
    }

    void onTick(uv_timer_t* handle)
    {
        return _tick_callback(handle);
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
    TimerCallback _tick_callback;
    TimerCallback _read_temperature_callback;

    Context() : _tick_callback(), _read_temperature_callback()
    {}
};
