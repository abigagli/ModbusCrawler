#pragma once
#include "modbus_types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <modbus.h>
#include <random>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace modbus {
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

} // namespace detail

class slave_concept
{
    slave_id_t id_;
    std::string name_;

public:
    [[nodiscard]] slave_id_t id() const noexcept { return id_; }
    [[nodiscard]] std::string const &name() const noexcept { return name_; }

    slave_concept(slave_id_t id, std::string name)
      : id_(id), name_(std::move(name))
    {}

    virtual ~slave_concept() = default;

    virtual intmax_t read_input_registers(int address,
                                          int regsize,
                                          word_endianess endianess)  = 0;
    virtual std::vector<uint16_t> read_input_registers(int address,
                                                       int num_regs) = 0;


    virtual intmax_t read_holding_registers(int address,
                                            int regsize,
                                            word_endianess endianess)  = 0;
    virtual std::vector<uint16_t> read_holding_registers(int address,
                                                         int num_regs) = 0;

    // Non-pure, as we don't require these to be implemented
    virtual void write_holding_register(int address, uint16_t value) {}

    virtual void write_multiple_registers(
      int address,
      std::vector<uint16_t> const &registers)
    {}

    virtual void write_multiple_registers(int address,
                                          uint16_t const *regs,
                                          int num_regs)
    {}
};

class slave
{
    std::unique_ptr<slave_concept> c;

public:
    template <class T>
    struct model_type
    {};

    template <class M, class... T>
    explicit slave(model_type<M>, T &&...args)
      : c(new M(std::forward<T>(args)...))
    {}

    [[nodiscard]] slave_id_t id() const noexcept { return c->id(); }
    [[nodiscard]] std::string const &name() const noexcept { return c->name(); }

    intmax_t read_input_registers(int address,
                                  int regsize,
                                  word_endianess endianess)
    {
        return c->read_input_registers(address, regsize, endianess);
    }
    std::vector<uint16_t> read_input_registers(int address, int num_regs)
    {
        return c->read_input_registers(address, num_regs);
    }


    intmax_t read_holding_registers(int address,
                                    int regsize,
                                    word_endianess endianess)
    {
        return c->read_holding_registers(address, regsize, endianess);
    }
    std::vector<uint16_t> read_holding_registers(int address, int num_regs)
    {
        return c->read_holding_registers(address, num_regs);
    }

    void write_holding_register(int address, uint16_t value)
    {
        c->write_holding_register(address, value);
    }

    void write_multiple_registers(int address,
                                  std::vector<uint16_t> const &registers)
    {
        c->write_multiple_registers(address, registers);
    }

    void write_multiple_registers(int address,
                                  uint16_t const *regs,
                                  int num_regs)
    {
        c->write_multiple_registers(address, regs, num_regs);
    }
};

class RandomSlave: public slave_concept
{
public:
    template <class T>
    class random_source
    {
        std::random_device r;
        std::normal_distribution<T> d;
        std::default_random_engine engine;

    public:
        random_source(T mean, T stdev) : d(mean, stdev), engine(r()) {}
        T operator()() { return d(engine); }
    };
    class random_params
    {
        friend class RandomSlave;
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
                throw std::invalid_argument("Invalid random config: " +
                                            iss.str());

            return std::tuple<double, double>{std::stod(parts[0]),
                                              std::stod(parts[1])};
        }

    public:
        explicit random_params(std::string const &random_config)
        {
            std::tie(mean_, stdev_) =
              unpack_random_config(std::istringstream(random_config));
        }
    };

    std::map<int, random_source<double>> fake_registers_;

    RandomSlave(slave_id_t server_id,
                std::string server_name,
                std::map<int, random_params> const &fake_regs_config,
                bool verbose = false)
      : slave_concept(server_id, std::move(server_name))
    {
        for (auto const &fr: fake_regs_config)
            fake_registers_.try_emplace(
              fr.first, fr.second.mean_, fr.second.stdev_);
    }

    intmax_t read_input_registers(int address, int, word_endianess) override
    {
        auto where = fake_registers_.find(address);
        if (where == std::end(fake_registers_))
            throw std::runtime_error(
              "no random source configured for address " +
              std::to_string(address));
        return where->second();
    }
    std::vector<uint16_t> read_input_registers(int address,
                                               int num_regs) override
    {
        std::vector<uint16_t> res;
        res.reserve(num_regs);

        auto const regsize_dontcare        = 0;
        auto const word_endianess_dontcare = word_endianess::little;

        for (auto i = 0; i != num_regs; ++i)
        {
            res.push_back(read_input_registers(
              address + i, regsize_dontcare, word_endianess_dontcare));
        }

        return res;
    }

    intmax_t read_holding_registers(int address, int, word_endianess) override
    {
        int const regsize_dontcare         = 0;
        auto const word_endianess_dontcare = word_endianess::little;

        return read_input_registers(
          address, regsize_dontcare, word_endianess_dontcare);
    }
    std::vector<uint16_t> read_holding_registers(int address,
                                                 int num_regs) override
    {
        return read_input_registers(address, num_regs);
    }
};

class RTUSlave: public slave_concept
{
    struct ctx_deleter
    {
        void operator()(modbus_t *ctx)
        {
            modbus_close(ctx);
            modbus_free(ctx);
        }
    };

    std::unique_ptr<modbus_t, ctx_deleter> modbus_source_;

public:
    class serial_line
    {
        friend class RTUSlave;
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
                throw std::invalid_argument("Invalid line config: " +
                                            iss.str());

            return std::tuple<int, int, char, int>{std::stoi(parts[0]),
                                                   std::stoi(parts[1]),
                                                   parts[2][0],
                                                   std::stoi(parts[3])};
        }

    public:
        serial_line(std::string device, std::string const &line_config)
          : device_(std::move(device))
        {
            std::tie(bps_, data_bits_, parity_, stop_bits_) =
              unpack_line_config(std::istringstream(line_config));
        }
    };

    RTUSlave(slave_id_t server_id,
             std::string server_name,
             serial_line const &serial_line,
             std::chrono::milliseconds const &answering_time,
             bool verbose = false)
      : slave_concept(server_id, std::move(server_name))
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
                                  word_endianess endianess) override
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
                                    word_endianess endianess) override
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

    std::vector<uint16_t> read_input_registers(int address,
                                               int regsize) override
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

    std::vector<uint16_t> read_holding_registers(int address,
                                                 int regsize) override
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


    void write_holding_register(int address, uint16_t value) override
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

    void write_multiple_registers(
      int address,
      std::vector<uint16_t> const &registers) override
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
                                  int num_regs) override
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
};

} // namespace modbus

#if defined(DOCTEST_LIBRARY_INCLUDED)
TEST_CASE("Random Slave Should respect params")
{
    std::map<int, modbus::RandomSlave::random_params> random_params{
      {1, modbus::RandomSlave::random_params("2000:100")}};

    modbus::slave s(modbus::slave::model_type<modbus::RandomSlave>{},
                    500,
                    "Testing Slave",
                    random_params,
                    false);
    auto val = s.read_holding_registers(1, 1, modbus::word_endianess::little);

    CAPTURE(val);
    bool const in_range = (val >= 2000 - 100) && (val <= 2000 + 100);
    CHECK(in_range);
}
#endif