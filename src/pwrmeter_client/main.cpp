#include "rtu_context.hpp"

#include <chrono>
#include <cinttypes>
#include <fstream>
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
                    [-d <device =/dev/ttyUSB0>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <register>
                    <regsize ={{1|2|4}{l|b} | Nr}>

                    |
                    -W
                    [-d <device = /dev/ttyUSB0>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <register>
                    <value [0..65535]>

                    |
                    -U
                    [-d <device = /dev/ttyUSB0>]
                    [-c <line_config ="9600:8:N:1">]
                    [-a <answering_timeout_ms =500>]
                    -s <server_id>
                    <filename>
                })"
              << std::endl;
    return res;
}

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
registers(std::string const &filename, uint32_t *maybe_crc = nullptr)
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

    std::clog << "read " << file_len << " bytes from " << filename << " into "
              << content.size() << " elements. CRC32 = " << std::hex
              << crc_value << std::dec;

    if (maybe_crc)
        *maybe_crc = crc_value;
    return content;
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
    std::string const device      = "/dev/ttyUSB0";
    std::string const line_config = "9600:8:N:1";
    auto const answering_time     = 500ms;
    bool verbose                  = false;
} // namespace defaults

mode_t mode;
std::string device      = defaults::device;
std::string line_config = defaults::line_config;
int server_id           = -1;
auto answering_time     = defaults::answering_time;
bool verbose            = defaults::verbose;
} // namespace options
#pragma clang diagnostic pop

int
single_read(int address, std::string regspec)
{
    assert(!regspec.empty());

    auto const last_char = regspec.back();
    if (regspec.size() < 2 ||
        (last_char != 'l' && last_char != 'b' && last_char != 'r'))
        return usage(-1, "invalid regsize specification: " + regspec);

    if (last_char == 'r')
    {
        // Raw read
        int const num_regs = std::strtol(regspec.c_str(), nullptr, 0);
        modbus::RTUContext ctx(
          options::server_id,
          "Server_" + std::to_string(options::server_id),
          modbus::SerialLine(options::device, options::line_config),
          options::answering_time,
          options::verbose);

        std::vector<uint16_t> registers =
          ctx.read_holding_registers(address, num_regs);

        for (auto r = 0ULL; r != registers.size(); ++r)
        {
            auto const cur_addr = address + r * sizeof(uint16_t);
            std::clog << "RAW READ: " << std::setw(8) << std::hex << cur_addr
                      << ": " << std::setw(8) << registers[r] << " (dec "
                      << std::dec << std::setw(10) << registers[r] << ")";
        }
    }
    else
    {
        int const regsize = regspec[0] - '0';
        modbus::word_endianess word_endianess;

        if (regsize != 1 && regsize != 2 && regsize != 4)
            return usage(-1, "regsize must be <= 4");

        word_endianess = regspec[1] == 'l' ? modbus::word_endianess::little
                                           : modbus::word_endianess::big;

        modbus::RTUContext ctx(
          options::server_id,
          "Server_" + std::to_string(options::server_id),
          modbus::SerialLine(options::device, options::line_config),
          options::answering_time,
          options::verbose);

        int64_t const val =
          ctx.read_holding_registers(address, regsize, word_endianess);

        std::clog << "SINGLE READ REGISTER " << address << ": " << val;
    }
    return 0;
}

int
single_write(int address, intmax_t value)
{
    if (value < 0 || value > std::numeric_limits<uint16_t>::max())
        return usage(-1, "invalid value: must be [0..65535]");

    modbus::RTUContext ctx(
      options::server_id,
      "Server_" + std::to_string(options::server_id),
      modbus::SerialLine(options::device, options::line_config),
      options::answering_time,
      options::verbose);

    ctx.write_holding_register(address, value);
    std::clog << "SINGLE WRITE REGISTER " << address << ": " << value;
    return 0;
}

int
flash_update(std::string filename)
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

    modbus::RTUContext ctx(
      options::server_id,
      "Server_" + std::to_string(options::server_id),
      modbus::SerialLine(options::device, options::line_config),
      options::answering_time,
      options::verbose);

    uint16_t const required_image_version = ctx.read_holding_registers(
      static_cast<int>(flash_update_registers::required_image_version),
      1,
      modbus::word_endianess::little);

    std::clog << "Device requires fw image " << required_image_version;

    filename += std::to_string(required_image_version) + ".bin";
    uint32_t checksum;
    std::vector<uint16_t> content = registers(filename, &checksum);

    if (content.size() > 3)
    {
        uint32_t const reset_vector = (content[3] << 16) | content[2];
        std::clog << "Requested image ResetHandler @" << std::hex
                  << reset_vector << std::dec;
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

    std::clog << "Sending 'start' command";
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::cmd),
      static_cast<uint16_t>(flash_update_commands::start));

    for (auto flash_line = 0ULL; flash_line != full_lines; ++flash_line)
    {
        std::clog << "FLASH line " << flash_line << " @ 0x" << std::hex
                  << flash_offset << ", REGBUFF @ 0x" << buffer_offset
                  << std::dec << ",  " << flash_line_bytes << " bytes in 2 * "
                  << modbus_regs_at_once << " registers";

        // Send current offset inside the receiver's pre-flash-write buffer,
        // i.e. flash_line * flash_line_bytes
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::offset_high),
          flash_offset >> 16);

        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::offset_low), flash_offset);

        // Send the current flash_line in two modbus' multiple-register writes
        ctx.write_multiple_registers(buffer_offset, regs, modbus_regs_at_once);
        ctx.write_multiple_registers(buffer_offset + modbus_regs_at_once,
                                     regs + modbus_regs_at_once,
                                     modbus_regs_at_once);

        // Send the actual bytes of the current flash line, in this case this is
        // always a full line
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::chunk_len),
          flash_line_bytes);

        // Send the write_segment command
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::cmd),
          static_cast<uint16_t>(flash_update_commands::write_segment));

        regs += 2 * modbus_regs_at_once;
        flash_offset += flash_line_bytes;
    }

    auto const remaining_bytes = total_len_bytes % flash_line_bytes;

    if (remaining_bytes)
    {
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::offset_high),
          flash_offset >> 16);

        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::offset_low), flash_offset);

        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::chunk_len), remaining_bytes);

        auto const remaining_chunks = remaining_bytes / 2 / modbus_regs_at_once;
        auto const remaining_regs = (remaining_bytes / 2) % modbus_regs_at_once;

        if (remaining_chunks)
        {
            std::clog << "FLASH remaining chunk @ 0x" << std::hex
                      << flash_offset << ", REGBUFF @ 0x" << buffer_offset
                      << std::dec << ",  " << modbus_regs_at_once * 2
                      << " bytes in " << modbus_regs_at_once << " registers";

            ctx.write_multiple_registers(
              buffer_offset, regs, modbus_regs_at_once);

            regs += modbus_regs_at_once;
            buffer_offset += modbus_regs_at_once;
        }

        std::clog << "FLASH remaining bytes @ 0x" << std::hex << flash_offset
                  << ", REGBUFF @ 0x" << buffer_offset << std::dec << ",  "
                  << remaining_regs * 2 << " bytes in " << remaining_regs
                  << " registers";

        ctx.write_multiple_registers(buffer_offset, regs, remaining_regs);

        // Send the write_segment command
        ctx.write_holding_register(
          static_cast<int>(flash_update_registers::cmd),
          static_cast<uint16_t>(flash_update_commands::write_segment));
    }
    std::clog << "Sending total len " << total_len_bytes;
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::total_len_high),
      total_len_bytes >> 16);
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::total_len_low), total_len_bytes);

    std::clog << "Sending crc32 " << std::hex << checksum << std::dec;
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::crc32_high), checksum >> 16);
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::crc32_low), checksum);

    std::clog << "Sending 'done' command";
    ctx.write_holding_register(
      static_cast<int>(flash_update_registers::cmd),
      static_cast<uint16_t>(flash_update_commands::done));

    std::clog << "FLASH UPDATE completed";
    return 0;
}

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
            options::device = optarg;
            break;
        case 'c':
            options::line_config = optarg;
            break;
        case 's':
            options::server_id = std::stoi(optarg);
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

    if (options::mode == options::mode_t::single_read)
    {
        if (options::server_id < 0 || argc < 2)
            return usage(-1,
                         "missing mandatory parameters for single_read mode");

        int const address   = std::strtol(argv[0], nullptr, 0);
        char const *regspec = argv[1];
        return single_read(address, regspec);
    }
    else if (options::mode == options::mode_t::single_write)
    {
        if (options::server_id < 0 || argc < 2)
            return usage(-1,
                         "missing mandatory parameters for single_write mode");

        int const address = std::strtol(argv[0], nullptr, 0);
        intmax_t value    = std::strtoimax(argv[1], nullptr, 0);
        return single_write(address, value);
    }
    else if (options::mode == options::mode_t::flash_update)
    {
        if (options::server_id < 0 || argc < 1)
            return usage(-1,
                         "missing mandatory parameters for flash_update mode");

        return flash_update(argv[0]);
    }
    else
        return usage(-1);
}
#pragma clang diagnostic pop