#include <modbus.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <map>
#include <string>


using namespace std::string_literals;
using json = nlohmann::json;

int usage (char const *name, int res)
{
    std::cout << "Usage:\n";
    std::cout << name << " <server_id> [serial_device]\n";
    return res;
}

int main(int argc, char *argv[])
{
    std::string dev = "/dev/ttyUSB0";

    if (argc < 2)
        return usage(argv[0], -1);

    int server_id = std::stoul (argv[1]);

    if (argc > 2)
        dev = argv[2];

    int address = std::stoul (argv[3]);
    int regsize = std::stoul (argv[4]);




    modbus_t *ctx = modbus_new_rtu(dev.c_str(), 9600, 'N', 8, 1);

    if (!ctx)
        throw std::runtime_error ("Failed creating ctx for device " + dev);

    int api_rv;
    api_rv = modbus_set_debug(ctx, TRUE);
    api_rv = modbus_set_error_recovery(ctx,
                              static_cast<modbus_error_recovery_mode>(
                                    MODBUS_ERROR_RECOVERY_LINK |
                                    MODBUS_ERROR_RECOVERY_PROTOCOL));

    api_rv = modbus_set_slave(ctx, server_id);
    api_rv = modbus_connect(ctx);

    if (api_rv < 0)
        throw std::runtime_error ("Connection failed: "s + modbus_strerror(errno));


    uint16_t regs[2];
    if (address >= 40000)
        api_rv = modbus_read_registers(ctx, address - 40000, regsize, regs);  // Holding register: Code 03
    else
        api_rv = modbus_read_input_registers(ctx, address, regsize, regs);    // Input register: Code 04


    int val;
    if (regsize == 1)
        val = regs[0];
    else
        val = static_cast<int>(regs[0]) << 16 | regs[1];

    std::cout << "API: " << api_rv << ", REGISTER: " << val << '\n';

    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}
