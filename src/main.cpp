#include <modbus.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>

using namespace std::string_literals;
using json = nlohmann::json;

namespace
{
    int usage (char const *name, int res)
    {
        std::cout << "Usage:\n";
        std::cout << name << " <server_id> [serial_device]\n";
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
        std::string line_config = "9600:8:N:1";
    }// namespace defaults

    // cmdline options
    std::string device = defaults::device;
    bool verbose = defaults::verbose;
    std::string line_config = defaults::line_config;
    int server_id;

    // cmdline params
    int address;
    int regsize;
}// namespace options

int main(int argc, char *argv[])
{
    optind = 0;

    for (auto ch = getopt(argc, argv, "hd:vl:s:"); ch != -1;)
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
            case '?':
            case 'h':
            default:
                return usage(argv[0], 0);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 2)
        return usage(argv[0], -1);

    options::address = std::stoi (argv[0]);
    options::regsize = std::stoi (argv[1]);

    auto [bps, data_bits, parity, stop_bits] = unpack_line_config (std::istringstream(options::line_config));

    modbus_t *ctx = modbus_new_rtu(options::device.c_str(), bps, parity, data_bits, stop_bits);

    if (!ctx)
        throw std::runtime_error ("Failed creating ctx for device " + options::device);

    int api_rv;
    api_rv = modbus_set_debug(ctx, TRUE);
    api_rv = modbus_set_error_recovery(ctx,
                              static_cast<modbus_error_recovery_mode>(
                                    MODBUS_ERROR_RECOVERY_LINK |
                                    MODBUS_ERROR_RECOVERY_PROTOCOL));

    api_rv = modbus_set_slave(ctx, options::server_id);
    api_rv = modbus_connect(ctx);

    if (api_rv < 0)
        throw std::runtime_error ("Connection failed: "s + modbus_strerror(errno));


    uint16_t regs[2];
    if (options::address >= 40000)
        api_rv = modbus_read_registers(ctx, options::address - 40000, options::regsize, regs);  // Holding register: Code 03
    else
        api_rv = modbus_read_input_registers(ctx, options::address, options::regsize, regs);    // Input register: Code 04


    int val;
    if (options::regsize == 1)
        val = regs[0];
    else
        val = static_cast<int>(regs[0]) << 16 | regs[1];

    std::cout << "API: " << api_rv << ", REGISTER: " << val << '\n';

    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}
