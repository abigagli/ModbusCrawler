#include "modbus_ops.h"

#include <chrono>
#include <cinttypes>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace std::chrono_literals;
namespace {
std::string g_prog_name;
int
usage(int res, std::string const &msg = "")
{
    if (!msg.empty())
        std::cerr << "\n*** ERROR: " << msg << " ***\n\n";
    std::cerr << "Usage:\n";
    std::cerr << g_prog_name << R"(
                [-h(help)]
                [-v(erbose)]
                {
                    -R
                    [-d <device = /dev/ttyCOM1>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <register>
                    <regsize ={{1|2|4}{l|b} | Nr}>

                    |
                    -W
                    [-d <device = /dev/ttyCOM1>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <register>
                    <value [0..65535]>

                    |
                    -U
                    [-d <device = /dev/ttyCOM1>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <filename>
                })"
              << std::endl;
    return res;
}

} // namespace

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"
namespace options {
enum class mode_t
{
    unknown,
    single_read,
    single_write,
    flash_update,
};
namespace defaults {
    mode_t const mode = mode_t::unknown;

    int const slave_id                             = -1;
    std::string const serial_device                = "/dev/ttyCOM1";
    std::string const serial_config                = "9600:8:N:1";
    std::chrono::milliseconds const answering_time = 500ms;

    bool verbose = false;
} // namespace defaults

auto mode = defaults::mode;
inline namespace rtu_parameters {
    auto slave_id       = defaults::slave_id;
    auto serial_device  = defaults::serial_device;
    auto serial_config  = defaults::serial_config;
    auto answering_time = defaults::answering_time;
} // namespace rtu_parameters

auto verbose = defaults::verbose;
} // namespace options
#pragma clang diagnostic pop


#pragma clang diagnostic push
#pragma ide diagnostic ignored "concurrency-mt-unsafe"
int
main(int argc, char *argv[])
{
    g_prog_name = argv[0];
    optind      = 1;
    int ch;
    while ((ch = getopt(argc, argv, "vURWd:c:s:a:h")) != -1)
    {
        switch (ch)
        {
        case 'v':
            options::verbose = true;
            break;
        case 'R':
            options::mode = options::mode_t::single_read;
            break;
        case 'W':
            options::mode = options::mode_t::single_write;
            break;
        case 'U':
            options::mode = options::mode_t::flash_update;
            break;
        case 'd':
            options::serial_device = optarg;
            break;
        case 'c':
            options::serial_config = optarg;
            break;
        case 's':
            options::slave_id = std::stoi(optarg);
            break;
        case 'a':
            options::answering_time =
              std::chrono::milliseconds(std::stoi(optarg));
            break;
        case '?':
            return usage(-1);
        case 'h':
        default:
            return usage(0);
        }
    }

    argc -= optind;
    argv += optind;

    modbus::rtu_parameters rtu_parameters{
      .slave_id       = options::slave_id,
      .serial_device  = options::serial_device,
      .serial_config  = options::serial_config,
      .answering_time = options::answering_time};

    try
    {
        if (options::mode == options::mode_t::single_read)
        {
            if (options::slave_id < 0 || argc < 2)
                return usage(
                  -1, "missing mandatory parameters for single_read mode");

            int const address   = std::strtol(argv[0], nullptr, 0);
            char const *regspec = argv[1];
            modbus::single_read(
              rtu_parameters, address, regspec, options::verbose);
            return 0;
        }
        else if (options::mode == options::mode_t::single_write)
        {
            if (options::slave_id < 0 || argc < 2)
                return usage(
                  -1, "missing mandatory parameters for single_write mode");

            int const address = std::strtol(argv[0], nullptr, 0);
            intmax_t value    = std::strtoimax(argv[1], nullptr, 0);
            modbus::single_write(
              rtu_parameters, address, value, options::verbose);
            return 0;
        }
        else if (options::mode == options::mode_t::flash_update)
        {
            if (options::slave_id < 0 || argc < 1)
                return usage(
                  -1, "missing mandatory parameters for flash_update mode");

            modbus::flash_update(rtu_parameters, argv[0], options::verbose);
            return 0;
        }
    }
    catch (std::invalid_argument const &e)
    {
        return usage(-1, e.what());
    }
    return usage(-1);
}
#pragma clang diagnostic pop
