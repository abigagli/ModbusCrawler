#pragma once

#include <modbus.h>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <tuple>
#include <cassert>
#include <chrono>
#include <functional>

namespace modbus {

inline int64_t
to_val(uint16_t const *regs, int regsize)
{
    int64_t val;
    switch (regsize)
    {
    case 1:
        val = regs[0];
        break;
    case 2:
        val = static_cast<int64_t>(regs[1]) << 16 | regs[0];
        break;
    case 3:
        val = static_cast<int64_t>(regs[2]) << 32 |
              static_cast<int64_t>(regs[1]) << 16 | regs[0];
        break;
    case 4:
        if (regs[3] > std::numeric_limits<int16_t>::max())
            throw std::overflow_error("MSB of 64bit value too big: " + std::to_string(regs[3]));

        val = static_cast<int64_t>(regs[3]) << 48 |
              static_cast<int64_t>(regs[2]) << 32 |
              static_cast<int64_t>(regs[1]) << 16 | regs[0];
        break;
    default:
        assert(!"regsize not supported");
    }

    return val;
}

class SerialLine
{
    friend class RTUContext;
    std::string device_;
    int bps_;
    int data_bits_;
    char parity_;
    int stop_bits_;

    auto unpack_line_config(std::istringstream iss)
    {
        std::vector<std::string> parts;
        std::string elem;
        while (std::getline(iss, elem, ':'))
        {
            parts.push_back(elem);
        }

        if (parts.size() != 4)
            throw std::runtime_error("Invalid line config: " + iss.str());

        return std::tuple<int, int, char, int>{std::stoi(parts[0]),
                                               std::stoi(parts[1]),
                                               parts[2][0],
                                               std::stoi(parts[3])};
    }

public:
    SerialLine(std::string device, std::string const &line_config)
      : device_(device)
    {
        std::tie(bps_, data_bits_, parity_, stop_bits_) =
          unpack_line_config(std::istringstream(line_config));
    }
};

using namespace std::string_literals;
class RTUContext
{
    struct ctx_deleter
    {
        void operator()(modbus_t *ctx)
        {
            modbus_close(ctx);
            modbus_free(ctx);
        }
    };

    std::unique_ptr<modbus_t, ctx_deleter> ctx_;

public:
    RTUContext(int server_id,
               SerialLine const &serial_line,
               std::chrono::milliseconds const &answering_time,
               bool verbose = false)
    {
        ctx_.reset(modbus_new_rtu(serial_line.device_.c_str(),
                                  serial_line.bps_,
                                  serial_line.parity_,
                                  serial_line.data_bits_,
                                  serial_line.stop_bits_));

        if (!ctx_)
            throw std::runtime_error("Failed creating ctx for device " +
                                     serial_line.device_);

        int api_rv;
        if (verbose)
            api_rv = modbus_set_debug(ctx_.get(), TRUE);

        api_rv = modbus_set_error_recovery(
          ctx_.get(),
          static_cast<modbus_error_recovery_mode>(
            MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL));

        auto seconds =
          std::chrono::duration_cast<std::chrono::seconds>(answering_time);
        auto microseconds = std::chrono::microseconds(answering_time - seconds);

        modbus_set_response_timeout(
          ctx_.get(), seconds.count(), microseconds.count());
        modbus_set_slave(ctx_.get(), server_id);

        if (modbus_connect(ctx_.get()) < 0)
            throw std::runtime_error("Failed modbus_connect: "s +
                                     modbus_strerror(errno));
    }

    int64_t read_input_registers(int address, int regsize)
    {
        if (regsize > 4)
            throw std::runtime_error("Invalid regsize: " +
                                     std::to_string(regsize));

        uint16_t regs[4]{};
        int api_rv =
          modbus_read_input_registers(ctx_.get(),
                                      address,
                                      regsize,
                                      regs); // Input register: Code 03

        if (api_rv != regsize)
            throw std::runtime_error("Failed modbus_read_input_registers: "s +
                                     modbus_strerror(errno));

        return to_val(regs, regsize);
    }

    int64_t read_holding_registers(int address, int regsize)
    {
        if (regsize > 4)
            throw std::runtime_error("Invalid regsize: " +
                                     std::to_string(regsize));

        uint16_t regs[4]{};
        int api_rv = modbus_read_registers(ctx_.get(),
                                           address,
                                           regsize,
                                           regs); // Holding register: Code 03

        if (api_rv != regsize)
            throw std::runtime_error("Failed modbus_read_registers: "s +
                                     modbus_strerror(errno));

        return to_val(regs, regsize);
    }

    template <class F, class... Args>
    auto call(F &&callable, Args &&...args)
    {
        return std::invoke(
          std::forward<F>(callable), ctx_.get(), std::forward<Args>(args)...);
    }

    modbus_t *native_handle() const { return ctx_.get(); }
};
} // namespace modbus
