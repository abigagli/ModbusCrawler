#include "modbus_ops.h"

#if defined(USE_LOGURU)
#    include <loguru.hpp>
#    define EOL
#else
#    define LOG_S(x) std::clog
#    define EOL << std::endl;
#endif
#include "modbus_slave.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
class CRC32
{
    uint32_t table[256];

public:
    CRC32()
    {
        uint32_t const polynomial = 0xEDB88320;
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (size_t j = 0; j < 8; j++)
            {
                if (c & 1)
                {
                    c = polynomial ^ (c >> 1);
                }
                else
                {
                    c >>= 1;
                }
            }
            table[i] = c;
        }
    }

    uint32_t update(uint32_t initial, const void *buf, size_t len)
    {
        uint32_t c       = initial ^ 0xFFFFFFFF;
        const uint8_t *u = static_cast<const uint8_t *>(buf);
        for (size_t i = 0; i < len; ++i)
        {
            c = table[(c ^ u[i]) & 0xFF] ^ (c >> 8);
        }
        return c ^ 0xFFFFFFFF;
    }

    uint32_t update(uint32_t initial, uint8_t byte)
    {
        uint32_t c = initial ^ 0xFFFFFFFF;
        c          = table[(c ^ byte) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFF;
    }
};

std::vector<uint16_t>
registers_from_file(std::string const &filename, uint32_t *maybe_crc = nullptr)
{
    std::ifstream ifs(filename, std::ios::binary);

    if (!ifs)
        throw std::runtime_error("invalid filename " + filename);

    ifs.ignore(std::numeric_limits<std::streamsize>::max());
    auto const file_len = ifs.gcount();
    ifs.clear();
    ifs.seekg(0, std::ios::beg);

    std::istreambuf_iterator<char> it{ifs}, ibe;

    std::vector<uint16_t> content;

    // Eventually we need a 4-byte aligned size
    content.reserve(((file_len + 3) / 4) * 4);

    CRC32 checksum;
    uint32_t crc_value = 0;
    uint8_t high_byte{};
    while (it != ibe)
    {
        high_byte = static_cast<uint8_t>(*it++);
        crc_value = checksum.update(crc_value, high_byte);

        uint8_t const low_byte = it != ibe ? static_cast<uint8_t>(*it++) : 0;
        crc_value              = checksum.update(crc_value, low_byte);

        uint16_t regval = (high_byte << 8) | low_byte;

        content.push_back(regval);
    }

    // And now ensure the whole thing is 4-byte aligned
    while (content.size() % 2)
    {
        crc_value = checksum.update(crc_value, 0);
        crc_value = checksum.update(crc_value, 0);
        content.push_back(0);
    }

    LOG_S(INFO) << "read " << file_len << " bytes from " << filename << " into "
                << content.size() << " elements. CRC32 = " << std::hex
                << crc_value << std::dec EOL;

    if (maybe_crc)
        *maybe_crc = crc_value;
    return content;
}
} // namespace

namespace modbus {

void
single_read(modbus::rtu_parameters const &rp,
            int address,
            std::string regspec,
            bool verbose)
{
    assert(!regspec.empty());

    auto const last_char = regspec.back();
    if (regspec.size() < 2 ||
        (last_char != 'l' && last_char != 'b' && last_char != 'r'))
        throw std::invalid_argument("invalid regsize specification: " +
                                    regspec);

    int const regsize = regspec[0] - '0';
    if (last_char != 'r' && regsize != 1 && regsize != 2 && regsize != 4)
        throw std::invalid_argument("regsize must be 1, 2 or 4");

    modbus::slave rtu_slave(
      compiler::undeduced<modbus::RTUSlave>{},
      rp.slave_id,
      "Server_" + std::to_string(rp.slave_id),
      modbus::RTUSlave::serial_line(rp.serial_device, rp.serial_config),
      rp.answering_time,
      verbose);

    if (last_char == 'r')
    {
        // Raw read
        int const num_regs = std::strtol(regspec.c_str(), nullptr, 0);
        std::vector<uint16_t> registers =
          rtu_slave.read_holding_registers(address, num_regs);

        for (auto r = 0ULL; r != registers.size(); ++r)
        {
            auto const cur_addr = address + r * sizeof(uint16_t);
            LOG_S(INFO) << "RAW READ: " << std::setw(8) << std::hex << cur_addr
                        << ": " << std::setw(8) << registers[r] << " (dec "
                        << std::dec << std::setw(10) << registers[r] << ")" EOL;
        }
    }
    else
    {
        modbus::word_endianess word_endianess;

        word_endianess = regspec[1] == 'l' ? modbus::word_endianess::little
                                           : modbus::word_endianess::big;

        int64_t const val =
          rtu_slave.read_holding_registers(address, regsize, word_endianess);

        LOG_S(INFO) << "SINGLE READ REGISTER " << address << ": " << val EOL;
    }
}

void
single_write(modbus::rtu_parameters const &rp,
             int address,
             intmax_t value,
             bool verbose)
{
    if (value < 0 || value > std::numeric_limits<uint16_t>::max())
        throw std::invalid_argument("invalid value: must be [0..65535]");

    modbus::slave rtu_slave(
      compiler::undeduced<modbus::RTUSlave>{},
      rp.slave_id,
      "Server_" + std::to_string(rp.slave_id),
      modbus::RTUSlave::serial_line(rp.serial_device, rp.serial_config),
      rp.answering_time,
      verbose);

    rtu_slave.write_holding_register(address, value);
    LOG_S(INFO) << "SINGLE WRITE REGISTER " << address << ": " << value EOL;
}

void
file_transfer(modbus::rtu_parameters const &rp,
              int address,
              std::string filename,
              bool verbose)
{
    std::vector<uint16_t> content = registers_from_file(filename);

    modbus::slave rtu_slave(
      compiler::undeduced<modbus::RTUSlave>{},
      rp.slave_id,
      "Server_" + std::to_string(rp.slave_id),
      modbus::RTUSlave::serial_line(rp.serial_device, rp.serial_config),
      rp.answering_time,
      verbose);

    rtu_slave.write_multiple_registers(address, content);
    LOG_S(INFO) << "FILE TRANSFER completed" EOL;
}

void
flash_update(modbus::rtu_parameters const &rp,
             std::string filename,
             bool verbose)
{
    enum class flash_update_registers : int
    {
        required_image_version = 2992,
        total_len_high         = 2993,
        total_len_low          = 2994,
        crc32_high             = 2995,
        crc32_low              = 2996,
        offset_high            = 2997,
        offset_low             = 2998,
        chunk_len              = 2999,
        buffer                 = 3000,
        cmd                    = 3128,
    };

    enum class flash_update_commands : uint16_t
    {
        start         = 0xE05D,
        write_segment = 0xF1A5,
        done          = 0xD01E,
    };

    modbus::slave rtu_slave(
      compiler::undeduced<modbus::RTUSlave>{},
      rp.slave_id,
      "Server_" + std::to_string(rp.slave_id),
      modbus::RTUSlave::serial_line(rp.serial_device, rp.serial_config),
      rp.answering_time,
      verbose);

    uint16_t const required_image_version = rtu_slave.read_holding_registers(
      static_cast<int>(flash_update_registers::required_image_version),
      1,
      modbus::word_endianess::little);

    LOG_S(INFO) << "Device requires fw image " << required_image_version EOL;

    filename += std::to_string(required_image_version) + ".bin";
    uint32_t checksum;
    std::vector<uint16_t> content = registers_from_file(filename, &checksum);

    if (content.size() > 3)
    {
        uint32_t const reset_vector = (content[3] << 16) | content[2];
        LOG_S(INFO) << "Requested image ResetHandler @" << std::hex
                    << reset_vector << std::dec EOL;
    }

    auto const total_len_bytes = content.size() * sizeof(uint16_t);

    uint16_t constexpr flash_line_bytes = 256;
    auto const full_lines               = total_len_bytes / flash_line_bytes;

    int constexpr modbus_regs_at_once =
      flash_line_bytes / 2 /
      sizeof(uint16_t); // 1/2 flash line i.e. 64 registers i.e. 128 bytes

    uint32_t flash_offset = 0;
    int buffer_offset     = static_cast<int>(flash_update_registers::buffer);
    auto const *regs      = content.data();

    LOG_S(INFO) << "Sending 'start' command" EOL;
    rtu_slave.write_holding_register(
      static_cast<int>(flash_update_registers::cmd),
      static_cast<uint16_t>(flash_update_commands::start));

    for (auto flash_line = 0ULL; flash_line != full_lines; ++flash_line)
    {
        LOG_S(INFO) << "FLASH line " << flash_line << " @ 0x" << std::hex
                    << flash_offset << ", REGBUFF @ 0x" << buffer_offset
                    << std::dec << ",  " << flash_line_bytes << " bytes in 2 * "
                    << modbus_regs_at_once << " registers" EOL;

        // Send current offset inside the receiver's pre-flash-write buffer,
        // i.e. flash_line * flash_line_bytes
        rtu_slave.write_holding_register(
          static_cast<int>(flash_update_registers::offset_high),
          flash_offset >> 16);

        rtu_slave.write_holding_register(
          static_cast<int>(flash_update_registers::offset_low), flash_offset);

        // Send the current flash_line in two modbus' multiple-register writes
        rtu_slave.write_multiple_registers(
          buffer_offset, regs, modbus_regs_at_once);
        rtu_slave.write_multiple_registers(buffer_offset + modbus_regs_at_once,
                                           regs + modbus_regs_at_once,
                                           modbus_regs_at_once);

        // Send the actual bytes of the current flash line, in this case this is
        // always a full line
        rtu_slave.write_holding_register(
          static_cast<int>(flash_update_registers::chunk_len),
          flash_line_bytes);

        // Send the write_segment command
        rtu_slave.write_holding_register(
          static_cast<int>(flash_update_registers::cmd),
          static_cast<uint16_t>(flash_update_commands::write_segment));

        regs += 2 * modbus_regs_at_once;
        flash_offset += flash_line_bytes;
    }

    auto const remaining_bytes = total_len_bytes % flash_line_bytes;

    if (remaining_bytes)
    {
        rtu_slave.write_holding_register(
          static_cast<int>(flash_update_registers::offset_high),
          flash_offset >> 16);

        rtu_slave.write_holding_register(
          static_cast<int>(flash_update_registers::offset_low), flash_offset);

        rtu_slave.write_holding_register(
          static_cast<int>(flash_update_registers::chunk_len), remaining_bytes);

        auto const remaining_chunks = remaining_bytes / 2 / modbus_regs_at_once;
        auto const remaining_regs = (remaining_bytes / 2) % modbus_regs_at_once;

        if (remaining_chunks)
        {
            LOG_S(INFO) << "FLASH remaining chunk @ 0x" << std::hex
                        << flash_offset << ", REGBUFF @ 0x" << buffer_offset
                        << std::dec << ",  " << modbus_regs_at_once * 2
                        << " bytes in " << modbus_regs_at_once
                        << " registers" EOL;

            rtu_slave.write_multiple_registers(
              buffer_offset, regs, modbus_regs_at_once);

            regs += modbus_regs_at_once;
            buffer_offset += modbus_regs_at_once;
        }

        LOG_S(INFO) << "FLASH remaining bytes @ 0x" << std::hex << flash_offset
                    << ", REGBUFF @ 0x" << buffer_offset << std::dec << ",  "
                    << remaining_regs * 2 << " bytes in " << remaining_regs
                    << " registers" EOL;

        rtu_slave.write_multiple_registers(buffer_offset, regs, remaining_regs);

        // Send the write_segment command
        rtu_slave.write_holding_register(
          static_cast<int>(flash_update_registers::cmd),
          static_cast<uint16_t>(flash_update_commands::write_segment));
    }
    LOG_S(INFO) << "Sending total len " << total_len_bytes EOL;
    rtu_slave.write_holding_register(
      static_cast<int>(flash_update_registers::total_len_high),
      total_len_bytes >> 16);
    rtu_slave.write_holding_register(
      static_cast<int>(flash_update_registers::total_len_low), total_len_bytes);

    LOG_S(INFO) << "Sending crc32 " << std::hex << checksum << std::dec EOL;
    rtu_slave.write_holding_register(
      static_cast<int>(flash_update_registers::crc32_high), checksum >> 16);
    rtu_slave.write_holding_register(
      static_cast<int>(flash_update_registers::crc32_low), checksum);

    LOG_S(INFO) << "Sending 'done' command" EOL;
    rtu_slave.write_holding_register(
      static_cast<int>(flash_update_registers::cmd),
      static_cast<uint16_t>(flash_update_commands::done));

    LOG_S(INFO) << "FLASH UPDATE completed" EOL;
}

} // namespace modbus