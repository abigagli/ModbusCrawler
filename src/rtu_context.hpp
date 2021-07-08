#pragma once
#include "modbus_types.h"

#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <modbus.h>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace modbus {

static_assert(sizeof(intmax_t) >= sizeof(int64_t));
namespace detail {
    struct word_be_tag
    {};
    struct word_le_tag
    {};

    inline bool regsize_supported(int regsize)
    {
        return regsize == 1 || regsize == 2 || regsize == 4;
    }

    // Always return signed type, it's up to the consumer to convert
    // to unsigned if desired, and that is a well defined conversion
    inline intmax_t to_val(uint16_t const *regs, int regsize, word_le_tag)
    {
        // Byte-level is big-endian as per modbus spec, and it is handled by
        // libmodbus. Here we handle word/register-level endianness, in this
        // case little: LSW->MSW
        intmax_t val;
        switch (regsize)
        {
            // NOTE: force conversion to signed counterpart (i.e. intxx_t)
            // before assigning to the wider intmax_t type, to ensure sign
            // extension is performed
        case 1: {
            val = static_cast<int16_t>(regs[0]);
        }
        break;
        case 2: {
            val = static_cast<int32_t>(regs[1]) << 16 | regs[0];
        }
        break;
        case 4: {
            val = static_cast<int64_t>(regs[3]) << 48 |
                  static_cast<int64_t>(regs[2]) << 32 |
                  static_cast<int64_t>(regs[1]) << 16 | regs[0];
        }
        break;
        default:
            assert(!"regsize not supported");
        }

        return val;
    }

    inline intmax_t to_val(uint16_t const *regs, int regsize, word_be_tag)
    {
        // Byte-level is big-endian as per modbus spec, and it is handled by
        // libmodbus. Here we handle word/register-level endianness, in this
        // case big: MSW->LSW
        intmax_t val;
        switch (regsize)
        {
            // NOTE: force conversion to signed counterpart before
            // assigning to the wider type, to ensure sign extension is
            // performed
        case 1:
            val = static_cast<int16_t>(regs[0]);
            break;
        case 2:
            val = static_cast<int32_t>(regs[0]) << 16 | regs[1];
            break;
        case 4:
            val = static_cast<int64_t>(regs[0]) << 48 |
                  static_cast<int64_t>(regs[1]) << 32 |
                  static_cast<int64_t>(regs[2]) << 16 | regs[3];
            break;
        default:
            assert(!"regsize not supported");
        }

        return val;
    }

    template <class T>
    class RandomSource
    {
        std::random_device r;
        std::normal_distribution<T> d;
        std::default_random_engine engine;

    public:
        RandomSource(T mean, T stdev) : d(mean, stdev), engine(r()) {}
        T operator()() { return d(engine); }
    };
} // namespace detail

class SerialLine
{
    friend class RTUContext;
    std::string device_;
    int bps_;
    int data_bits_;
    char parity_;
    int stop_bits_;

    static auto unpack_line_config(std::istringstream iss)
    {
        std::vector<std::string> parts;
        std::string elem;
        while (std::getline(iss, elem, ':'))
        {
            parts.push_back(elem);
        }

        if (parts.size() != 4)
            throw std::invalid_argument("Invalid line config: " + iss.str());

        return std::tuple<int, int, char, int>{std::stoi(parts[0]),
                                               std::stoi(parts[1]),
                                               parts[2][0],
                                               std::stoi(parts[3])};
    }

public:
    SerialLine(std::string device, std::string const &line_config)
      : device_(std::move(device))
    {
        std::tie(bps_, data_bits_, parity_, stop_bits_) =
          unpack_line_config(std::istringstream(line_config));
    }
};

class RandomParams
{
    friend class RTUContext;
    double mean_;
    double stdev_;

    static auto unpack_random_config(std::istringstream iss)
    {
        std::vector<std::string> parts;
        std::string elem;
        while (std::getline(iss, elem, ':'))
        {
            parts.push_back(elem);
        }

        if (parts.size() != 2)
            throw std::invalid_argument("Invalid random config: " + iss.str());

        return std::tuple<double, double>{std::stod(parts[0]),
                                          std::stod(parts[1])};
    }

public:
    explicit RandomParams(std::string const &random_config)
    {
        std::tie(mean_, stdev_) =
          unpack_random_config(std::istringstream(random_config));
    }
};

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

    int modbus_id_;
    std::string server_name_;


    std::unique_ptr<modbus_t, ctx_deleter> modbus_source_;
    std::unique_ptr<detail::RandomSource<double>> random_source_;

    RTUContext(int server_id, std::string server_name)
      : modbus_id_(server_id), server_name_(std::move(server_name))
    {}

public:
    [[nodiscard]] std::string const &name() const noexcept
    {
        return server_name_;
    }

    [[nodiscard]] int id() const noexcept { return modbus_id_; }

    RTUContext(int server_id,
               std::string server_name,
               RandomParams const &random_params,
               bool verbose = false)
      : RTUContext(server_id, std::move(server_name))
    {
        random_source_.reset(new decltype(random_source_)::element_type(
          random_params.mean_, random_params.stdev_));
    }


    RTUContext(int server_id,
               std::string server_name,
               SerialLine const &serial_line,
               std::chrono::milliseconds const &answering_time,
               bool verbose = false)
      : RTUContext(server_id, std::move(server_name))
    {
        modbus_source_.reset(modbus_new_rtu(serial_line.device_.c_str(),
                                            serial_line.bps_,
                                            serial_line.parity_,
                                            serial_line.data_bits_,
                                            serial_line.stop_bits_));

        if (!modbus_source_)
            throw std::runtime_error("Failed creating ctx for device " +
                                     serial_line.device_);

        int api_rv;
        if (verbose)
            api_rv = modbus_set_debug(modbus_source_.get(), TRUE);

        api_rv = modbus_set_error_recovery(
          modbus_source_.get(),
          static_cast<modbus_error_recovery_mode>(
            MODBUS_ERROR_RECOVERY_LINK | MODBUS_ERROR_RECOVERY_PROTOCOL));

        auto seconds =
          std::chrono::duration_cast<std::chrono::seconds>(answering_time);
        auto microseconds = std::chrono::microseconds(answering_time - seconds);

        modbus_set_response_timeout(
          modbus_source_.get(), seconds.count(), microseconds.count());
        modbus_set_slave(modbus_source_.get(), server_id);

        if (modbus_connect(modbus_source_.get()) < 0)
            throw std::runtime_error(std::string("Failed modbus_connect: ") +
                                     modbus_strerror(errno));
    }

    intmax_t read_input_registers(int address,
                                  int regsize,
                                  word_endianess endianess)
    {
        if (!detail::regsize_supported(regsize))
            throw std::invalid_argument("Invalid regsize: " +
                                        std::to_string(regsize));

        uint16_t regs[4]{};
        int api_rv =
          modbus_read_input_registers(modbus_source_.get(),
                                      address,
                                      regsize,
                                      regs); // Input register: Code 0x04

        if (api_rv != regsize)
            throw std::runtime_error(
              std::string("Failed modbus_read_input_registers: ") +
              modbus_strerror(errno));

        return endianess == word_endianess::little
                 ? to_val(regs, regsize, detail::word_le_tag{})
                 : to_val(regs, regsize, detail::word_be_tag{});
    }

    intmax_t read_holding_registers(int address,
                                    int regsize,
                                    word_endianess endianess)
    {
        if (!detail::regsize_supported(regsize))
            throw std::invalid_argument("Invalid regsize: " +
                                        std::to_string(regsize));

        uint16_t regs[4]{};
        int api_rv = modbus_read_registers(modbus_source_.get(),
                                           address,
                                           regsize,
                                           regs); // Holding register: Code 0x03

        if (api_rv != regsize)
            throw std::runtime_error(
              std::string("Failed modbus_read_registers: ") +
              modbus_strerror(errno));

        return endianess == word_endianess::little
                 ? to_val(regs, regsize, detail::word_le_tag{})
                 : to_val(regs, regsize, detail::word_be_tag{});
    }

    std::vector<uint16_t> read_input_registers(int address, int regsize)
    {
        std::vector<uint16_t> registers(regsize);
        int api_rv = modbus_read_input_registers(
          modbus_source_.get(),
          address,
          regsize,
          registers.data()); // Holding register: Code 0x03

        if (api_rv != regsize)
            throw std::runtime_error(
              std::string("Failed modbus_read_input_registers: ") +
              modbus_strerror(errno));

        return registers;
    }

    std::vector<uint16_t> read_holding_registers(int address, int regsize)
    {
        std::vector<uint16_t> registers(regsize);
        int api_rv = modbus_read_registers(
          modbus_source_.get(),
          address,
          regsize,
          registers.data()); // Holding register: Code 0x03

        if (api_rv != regsize)
            throw std::runtime_error(
              std::string("Failed modbus_read_registers: ") +
              modbus_strerror(errno));

        return registers;
    }


    void write_holding_register(int address, uint16_t value)
    {
        int api_rv =
          modbus_write_register(modbus_source_.get(),
                                address,
                                value); // Write Holding Register: Code 0x06

        if (api_rv != 1)
            throw std::runtime_error(
              std::string("Failed modbus_write_register: ") +
              modbus_strerror(errno));
    }

    void write_multiple_registers(int address,
                                  std::vector<uint16_t> const &registers)
    {
        auto const chunks    = registers.size() / MODBUS_MAX_WRITE_REGISTERS;
        auto const remaining = registers.size() % MODBUS_MAX_WRITE_REGISTERS;

        int api_rv;
        uint16_t const *regs = registers.data();
        for (auto chunk = 0ULL; chunk != chunks; ++chunk)
        {
            api_rv = modbus_write_registers(
              modbus_source_.get(),
              address,
              MODBUS_MAX_WRITE_REGISTERS,
              regs); // Write Multiple Registers: Code 0x10
            if (api_rv != MODBUS_MAX_WRITE_REGISTERS)
                throw std::runtime_error(
                  std::string("Failed modbus_write_registers chunk #") +
                  std::to_string(chunk) + ": " + modbus_strerror(errno));

            address += MODBUS_MAX_WRITE_REGISTERS;
            regs += MODBUS_MAX_WRITE_REGISTERS;
        }

        api_rv =
          modbus_write_registers(modbus_source_.get(),
                                 address,
                                 remaining,
                                 regs); // Write Multiple Registers: Code 0x10
        if (api_rv != remaining)
            throw std::runtime_error(
              std::string("Failed modbus_write_registers remaining: ") +
              modbus_strerror(errno));
    }

    void write_multiple_registers(int address,
                                  uint16_t const *regs,
                                  int num_regs)
    {
        int const api_rv =
          modbus_write_registers(modbus_source_.get(),
                                 address,
                                 num_regs,
                                 regs); // Write Multiple Registers: Code 0x10

        if (api_rv != num_regs)
            throw std::runtime_error(
              std::string("Failed modbus_write_registers: ") +
              modbus_strerror(errno));
    }

    [[nodiscard]] auto read_random_value() const { return (*random_source_)(); }

    template <class F, class... Args>
    auto native_call(F &&callable, Args &&...args)
    {
        return std::invoke(std::forward<F>(callable),
                           modbus_source_.get(),
                           std::forward<Args>(args)...);
    }

    [[nodiscard]] modbus_t *native_handle() const noexcept
    {
        return modbus_source_.get();
    }
};
} // namespace modbus
