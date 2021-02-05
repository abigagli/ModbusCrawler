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
                [-v(erbose)]
                {
                    -m <measconfig_file.json>

                    | 

                    [-d <device = /dev/ttyUSB0>]
                    [-l <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <regnum>
                    <regsize <=4l/4b}>
                })"
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

    // Measure mode
    std::string measconfig_file;

    // Single-shot reads
    std::string device = defaults::device;
    bool verbose = defaults::verbose;
    std::string line_config = defaults::line_config;
    int server_id = -1;
    auto answering_time = defaults::answering_time;

    int address;
    int regsize;
    modbus::word_endianess word_endianess;
}// namespace options

int single_read (std::string const &prog_name, int argc, char *argv[])
{
    if (options::server_id < 0 || argc < 2)
        return usage(prog_name, -1, "missing mandatory parameters");

    options::address = std::stoi (argv[0]);

    std::string regspec = argv[1];
    options::regsize = regspec[0] - '0';

    if (options::regsize > 1)
    {
        if (options::regsize > 4)
            return usage(prog_name, -1, "regsize must be <= 4");

        if (regspec.size() != 2 || (regspec[1] != 'l' && regspec[1] != 'b'))
            return usage(prog_name, -1, "invalid regsize specification: " + regspec);

        options::word_endianess = regspec[1] == 'l'
                                    ? modbus::word_endianess::little
                                    : modbus::word_endianess::big;
    }
    else
        options::word_endianess = modbus::word_endianess::dontcare;

    modbus::RTUContext ctx(options::server_id,
                            modbus::SerialLine(options::device, options::line_config),
                           options::answering_time,
                           options::verbose);

    // if (options::address >= 40000)
    //     options::address -= 40000;

    int64_t const val = ctx.read_holding_registers (options::address, options::regsize, options::word_endianess);

    std::cout << "REGISTER: " << val << '\n';
    return 0;
}

int main(int argc, char *argv[])
{
    optind = 1;
    int ch;
    while ((ch = getopt(argc, argv, "hd:vl:s:a:m:")) != -1)
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
            case 'm':
                options::measconfig_file = optarg;
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

    if (options::measconfig_file.empty())
        return single_read (prog_name, argc, argv);

    return 0;
}