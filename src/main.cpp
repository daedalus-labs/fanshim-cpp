#include "fanshim/driver.hpp"
#include "fanshim/logger.hpp"

#include <signal.h>
#include <spdlog/spdlog.h>
#include <uv.h>

#include <iostream>


static void clearLoop()
{
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    int32_t result = uv_loop_close(uv_default_loop());
    if (result) {
        logger().error("Failed to close event loop: {}", uv_strerror(result));
    }
    logger().warn("Event loop closed");
}

static void walkAndClose(uv_handle_t* handle, void* /* unused */)
{
    uv_close(handle, [](uv_handle_t* /* unused */) {});
}

static void onSignalReceived(uv_signal_t* handle, int32_t signal)
{
    logger().warn("Signal received: {}", signal);
    if (!handle) {
        logger().warn("Signal handler is null");
        return;
    }

    int32_t result = uv_loop_close(handle->loop);
    if (result == UV_EBUSY) {
        uv_stop(handle->loop);
        uv_walk(handle->loop, walkAndClose, NULL);
    }
}

int main(int argc, char** argv)
{
    uv_signal_t sigint;
    uv_signal_init(uv_default_loop(), &sigint);
    uv_signal_start(&sigint, onSignalReceived, SIGINT);

    Configuration config;
    Driver driver(config);

    driver.run();
    clearLoop();
    return 0;
}
