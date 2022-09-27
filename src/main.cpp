#include "fanshim/driver.hpp"

#include <signal.h>
#include <spdlog/spdlog.h>

#include <iostream>


void signalHandler(int signum)
{
    cout << "Signal: " << signum << endl;

    if (signum == SIGTERM || signum == SIGINT) {
        set_led(1, 3, 1, 1, true);
        cout << "closed" << endl;
        exit(0);
    }
}

int main(int argc, char **argv)
{}
