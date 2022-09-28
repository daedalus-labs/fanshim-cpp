#include <gpiod.hpp>
#include <json.hpp>
#include <time.h>

#include <algorithm>
#include <cmath>
#include <csignal>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int br_counter = 0;

gpiod::chip rchip;
gpiod::line ln_fan, ln_led_clk, ln_led_dat;


//////////////////////////////////////////////////////////////////////////////////////////
// https://github.com/pimoroni/fanshim-python/issues/19#issuecomment-517478717
//////////////////////////////////////////////////////////////////////////////////////////
inline static void write_byte(uint8_t byte)
{
    for (int n = 0; n < 8; n++) {
        ln_led_dat.set_value((byte & (1 << (7 - n))) > 0);
        ln_led_clk.set_value(HIGH);
        // nano_usleep_frac(CLCK_STRETCH);
        ln_led_clk.set_value(LOW);
        // nano_usleep_frac(CLCK_STRETCH);
    }
}

void set_led(double tmp, int br, int hi, int lo, bool off = false)
{
    int r = 0, g = 0, b = 0;

    if (off) {
        r = 0;
        g = 0;
        b = 190;
    }
    else if (br != 0) {
        double s, v;
        s = 1;
        v = br / 31.0;
        //// hsv: hue from temperature; s set to 1, v set to brightness like the official code
        /// https://github.com/pimoroni/fanshim-python/blob/5841386d252a80eeac4155e596d75ef01f86b1cf/examples/automatic.py#L44

        vector<int> rgb = hsv2rgb(tmp2hue(tmp, hi, lo), s, v);
        r = rgb.at(0);
        g = rgb.at(1);
        b = rgb.at(2);
    }


    // start frame
    ln_led_dat.set_value(LOW);
    for (int i = 0; i < 32; ++i) {
        ln_led_clk.set_value(HIGH);
        // nano_usleep_frac(CLCK_STRETCH);
        ln_led_clk.set_value(LOW);
        // nano_usleep_frac(CLCK_STRETCH);
    }

    // A 32 bit LED frame for each LED in the string (<0xE0+brightness> <blue> <green> <red>)
    write_byte(0b11100000 | br); // in range of 0..31 for the fanshim
    write_byte(b);               // b
    write_byte(g);               // g
    write_byte(r);               // r

    // An end frame consisting of at least (n/2) bits of 1, where n is the number of LEDs in the string
    ln_led_dat.set_value(HIGH);
    for (int i = 0; i < 1; ++i) {
        ln_led_clk.set_value(HIGH);
        // nano_usleep_frac(CLCK_STRETCH);
        ln_led_clk.set_value(LOW);
        // nano_usleep_frac(CLCK_STRETCH);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

void blk_led(double tmp, int br, int on_threshold, int off_threshold, int delay)
{
    struct timespec ti;
    ti.tv_sec = 0;
    ti.tv_nsec = 500 * 1000 * 1000;
    for (int i = 1; i <= delay; i++) {
        // set_led(tmp, ( (i % 2) * br ), on_threshold, off_threshold);
        set_led(tmp, br, on_threshold, off_threshold);
        nanosleep(&ti, NULL);
        set_led(tmp, 0, on_threshold, off_threshold);
        nanosleep(&ti, NULL);
    }
}

void breath_led(double tmp, int brth, int on_threshold, int off_threshold, int delay, int* brs)
{
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = (long)(100 * 1000 * 1000);

    for (int i = 0; i < 10 * delay; i++) {
        // cout<<brs[br_counter]<<endl;
        set_led(tmp, brs[br_counter], on_threshold, off_threshold);
        if (br_counter >= 2 * brth - 1)
            br_counter = 0;
        else
            br_counter++;
        nanosleep(&req, NULL);
    }
}


int main(void)
{
    signal(SIGINT, signalHandler);
    gpiod::line_request lrq({"fanshim", gpiod::line_request::DIRECTION_OUTPUT, 0});

    try {
        const string chipname = "gpiochip0";

        rchip = gpiod::chip(chipname, gpiod::chip::OPEN_BY_NAME);

        ln_fan = rchip.get_line(fanshim_pin);
        ln_fan.request(lrq, 0);

        ln_led_dat = rchip.get_line(led_dat_pin);
        ln_led_dat.request(lrq, 0);

        ln_led_clk = rchip.get_line(led_clck_pin);
        ln_led_clk.request(lrq, 0);
    }
    catch (...) {
        cout << "init error" << endl;
    }
    cout << "fanshim init." << endl;


    map<string, int> fs_conf = get_fs_conf();


    const int delay_sec = fs_conf["delay"];
    const int on_threshold = fs_conf["on-threshold"];
    const int off_threshold = fs_conf["off-threshold"];
    const int budget = fs_conf["budget"];

    const struct timespec sleep_delay
    {
        delay_sec, 0L
    };

    int read_fs_pin = 0;

    fstream tmp_file;
    float tmp = 0;
    deque<int> tmp_q(budget, 0.0);
    int j;
    bool all_low, all_high;

    tmp_file.open("/sys/class/thermal/thermal_zone0/temp", ios_base::in);


    /// override file
    filesystem::path override_fp("/usr/local/etc/.force_fanshim");


    /// led
    int br = fs_conf["brightness"];
    int brt_br = fs_conf["breath_brgt"];
    int* brs = new int[brt_br * 2];

    for (int i = 0; i < brt_br * 2; i++) {
        if (i <= brt_br)
            brs[i] = i;
        else
            brs[i] = 2 * brt_br - i;
    }


    if (br > 31) {
        br = 31;
        cout << "brightness exceeds max = 31, set to max" << endl;
    }
    else if (br < 0) {
        br = 0;
        cout << "brightness lower than min = 0, set to 0" << endl;
    }


    if (br == 0) {
        set_led(0, 0, on_threshold, off_threshold);
    }


    while (1) {
        tmp_file >> tmp;
        tmp_file.seekg(0, tmp_file.beg);
        tmp = tmp / 1000;
        tmp_q.push_back(int(tmp));
        tmp_q.pop_front();
        deque<int>(tmp_q).swap(tmp_q);

        cout << "Temp: " << tmp << ", last " << budget << ": [ ";
        for (j = 0; j < tmp_q.size(); j++) {
            cout << tmp_q.at(j) << " ";
        }
        cout << "]\n";

        all_low = all_of(tmp_q.begin(), tmp_q.end(), [=](int tx) { return tx < off_threshold; });
        all_high = all_of(tmp_q.begin(), tmp_q.end(), [=](int tx) { return tx > on_threshold; });

        cout << "all low: " << boolalpha << all_low << "; ";
        cout << "all high: " << boolalpha << all_high << endl;

        // override
        if (filesystem::exists(override_fp)) {
            all_high = true;
            all_low = false;
            cout << "forcing fan on: override effective." << endl;
        }

        read_fs_pin = ln_fan.get_value();

        if (all_high && read_fs_pin == LOW) {
            ln_fan.set_value(HIGH);
        }
        else {
            if (all_low && read_fs_pin == HIGH) {
                ln_fan.set_value(LOW);
            }
        }


        read_fs_pin = ln_fan.get_value();
        cout << "fan state now: " << (read_fs_pin == LOW ? "[off]" : "[on]") << endl;

        ofstream nodex_fs;
        nodex_fs.open("/usr/local/etc/node_exp_txt/cpu_fan.prom");
        nodex_out = node_hdr + to_string(read_fs_pin) + "\n";
        nodex_out += node_hdr_t + to_string(int(tmp)) + "\n";
        nodex_fs << nodex_out;
        nodex_fs.close();


        /// set led
        if (br != 0) {
            if (fs_conf["blink"] != 0 && read_fs_pin == LOW) {
                if (fs_conf["blink"] == 1)
                    blk_led(tmp, br, on_threshold, off_threshold, delay_sec);
                else if (fs_conf["blink"] == 2)
                    breath_led(tmp, brt_br, on_threshold, off_threshold, delay_sec, brs);
            }
            else {
                set_led(tmp, br, on_threshold, off_threshold);
                nanosleep(&sleep_delay, NULL);
            }
        }
    }

    return 0;
}
