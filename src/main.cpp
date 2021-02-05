#include <modbus.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <unistd.h>

using namespace std::string_literals;
using namespace std::chrono_literals;
using json = nlohmann::json;

namespace
{
    int usage (std::string const &prog_name, int res, std::string const &msg = "")
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
                <regsize ={1|2|4}>)" << std::endl;
        return res;
    }

    auto unpack_line_config (std::istringstream iss)
    {
        std::vector<std::string> parts;
        std::string elem;
        while (std::getline(iss, elem, ':'))
        {
            parts.push_back(elem);
        }

        if (parts.size() != 4)
            throw std::runtime_error("Invalid line config: " + iss.str());

        return std::tuple<int, int, char, int>{std::stoi(parts[0]), std::stoi(parts[1]), parts[2][0], std::stoi(parts[3])};
    }

}// namespace unnamed

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


    auto [bps, data_bits, parity, stop_bits] = unpack_line_config (std::istringstream(options::line_config));

    modbus_t *ctx = modbus_new_rtu(options::device.c_str(), bps, parity, data_bits, stop_bits);

    if (!ctx)
        throw std::runtime_error ("Failed creating ctx for device " + options::device);

    int api_rv;
    if (options::verbose)
        api_rv = modbus_set_debug(ctx, TRUE);

    api_rv = modbus_set_error_recovery(ctx,
                              static_cast<modbus_error_recovery_mode>(
                                    MODBUS_ERROR_RECOVERY_LINK |
                                    MODBUS_ERROR_RECOVERY_PROTOCOL));

    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(options::answering_time);
    auto microseconds = std::chrono::microseconds(options::answering_time - seconds);

    modbus_set_response_timeout(ctx, seconds.count(), microseconds.count());
    modbus_set_slave(ctx, options::server_id);

    if (modbus_connect(ctx) < 0)
        throw std::runtime_error ("Failed modbus_connect: "s + modbus_strerror(errno));

    // if (options::address >= 40000)
    //     options::address -= 40000;

    uint16_t regs[4];
    api_rv = modbus_read_registers(ctx, options::address, options::regsize, regs);  // Holding register: Code 03

    if (api_rv != options::regsize)
        throw std::runtime_error ("Failed modbus_read_registers: "s + modbus_strerror(errno));

    //api_rv = modbus_read_input_registers(ctx, options::address, options::regsize, regs);    // Input register: Code 04

    int64_t val;
    switch (options::regsize)
    {
        case 1:
            val = regs[0];
            break;
        case 2:
            val = MODBUS_GET_INT32_FROM_INT16(regs, 0);
            break;
        case 4:
            val = MODBUS_GET_INT64_FROM_INT16(regs, 0);
            break;
        default:
            assert (!"Unreachable");
    }

    std::cout << "API: " << api_rv << ", REGISTER: " << val << '\n';

    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}
