#include "fanshim/configuration.hpp"

#include <json.hpp>

#include <map>
#include <string>


std::map<std::string, int> getConfiguration()
{
    std::map<std::string, int> fs_conf_default{{"on-threshold", 60}, {"off-threshold", 50}, {"budget", 3}, {"delay", 10}, {"brightness", 0}, {"blink", 0}, {"breath_brgt", 10}};

    std::map<std::string, int> fs_conf = fs_conf_default;

    try {
        ifstream fs_cfg_file("/usr/local/etc/fanshim.json");
        json fs_cfg_custom;
        fs_cfg_file >> fs_cfg_custom;

        for (auto &el : fs_cfg_custom.items()) {
            fs_conf[el.key()] = el.value();
        }

        if ((fs_conf["on-threshold"] <= fs_conf["off-threshold"]) || (fs_conf["budget"] <= 0) || (fs_conf["delay"] <= 0) || (fs_conf["breath_brgt"] <= 0) ||
            (fs_conf["breath_brgt"] > 31) || fs_conf["blink"] < 0 || fs_conf["blink"] > 2) {
            throw runtime_error("sanity check");
        }
    }
    catch (exception &e) {
        cout << "error parsing config file: " << e.what() << endl;
        fs_conf = fs_conf_default;
    }

    for (std::map<string, int>::iterator it = fs_conf.begin(); it != fs_conf.end(); ++it)
        cout << it->first << " => " << it->second << endl;

    return fs_conf;
}
