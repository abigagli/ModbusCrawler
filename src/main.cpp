#include "context.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <unistd.h>

using namespace std::chrono_literals;
using json = nlohmann::json;

namespace {

int
usage(std::string const &prog_name, int res, std::string const &msg = "")
{
    if (!msg.empty())
        std::cout << "\n*** ERROR: " << msg << " ***\n\n";
    std::cout << "Usage:\n";
    std::cout << prog_name << R"(
                [-h(help)]
                [-d <device = /dev/ttyUSB0>]
                [-v(erbose)]
                [-l <line_config ="9600:8:N:1">]
                [-a <answering_timeout_ms =500>]
                -s <server_id>
                <regnum>
                <regsize ={1|2|4}>)"
              << std::endl;
    return res;
}
} // namespace

namespace options
{
    namespace defaults
    {
        std::string const device = "/dev/ttyUSB0";
        bool const verbose = false;
        std::string const line_config = "9600:8:N:1";
        auto const answering_time = 500ms;
    }// namespace defaults

    // cmdline options
    std::string device = defaults::device;
    bool verbose = defaults::verbose;
    std::string line_config = defaults::line_config;
    int server_id = -1;
    auto answering_time = defaults::answering_time;

    // cmdline params
    int address;
    int regsize;
}// namespace options

int main(int argc, char *argv[])
{
    optind = 1;
    int ch;
    while ((ch = getopt(argc, argv, "hd:vl:s:a:")) != -1)
    {
        switch (ch)
        {
            case 'd':
                options::device = optarg;
                break;
            case 'v':
                options::verbose = true;
                break;
            case 'l':
                options::line_config = optarg;
                break;
            case 's':
                options::server_id = std::stoi(optarg);
                break;
            case 'a':
                options::answering_time = std::chrono::milliseconds(std::stoi(optarg));
                break;
            case '?':
                return usage(argv[0], -1);
                break;
            case 'h':
            default:
                return usage(argv[0], 0);
        }
    }

    auto const *prog_name = argv[0];
    argc -= optind;
    argv += optind;

    if (options::server_id < 0 || argc < 2)
        return usage(prog_name, -1, "missing mandatory parameters");

    options::address = std::stoi (argv[0]);
    options::regsize = std::stoi (argv[1]);

    if (options::regsize != 1 &&
        options::regsize != 2 &&
        options::regsize != 4)
        return usage(prog_name, -1, "regsize allowed values: {1 | 2 | 4}");

    auto serial_line = modbus::SerialLine(options::device, options::line_config);
    modbus::RTUContext ctx(options::server_id, serial_line, options::answering_time, options::verbose);

    // if (options::address >= 40000)
    //     options::address -= 40000;

    int64_t const val = ctx.read_holding_registers (options::address, options::regsize);

    std::cout << "REGISTER: " << val << '\n';

    return 0;
}