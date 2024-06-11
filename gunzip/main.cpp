#include <bitset>
#include <boost/locale.hpp>
#include <boost/locale/encoding.hpp>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <vector>

struct HexCharStruct {
  unsigned char c;
  HexCharStruct(unsigned char _c) : c(_c) {}
};

inline std::ostream &operator<<(std::ostream &o, const HexCharStruct &hs) {
  return (o << std::hex << (int)hs.c << std::dec);
}

inline HexCharStruct hex(unsigned char _c) { return HexCharStruct(_c); }

std::vector<char> read_n_bytes(std::ifstream &in, int n_bytes) {
  std::vector<char> buffer(n_bytes);
  in.read(buffer.data(), n_bytes);
  return buffer;
}

std::vector<char> read_until(std::ifstream &in, char until) {
  std::vector<char> buffer(0);
  char single_char;
  do {
    in.read(&single_char, 1);
    buffer.push_back(single_char);
  } while (single_char != until);
  return buffer;
}

void dump_bytes(std::vector<char> bytes) {
  for (auto val : bytes)
    std::cout << "0x" << hex(val) << ' ';
  std::cout << std::endl;
}

void dump_compression_method(char compression_method) {
  std::cout << hex(compression_method);
  if (compression_method == 0x08) {
    std::cout << " // deflate";
  } else {
    std::cout << " // unknown";
  }
  std::cout << std::endl;
}

struct Flags {
  bool ftext;
  bool fhcrc;
  bool fextra;
  bool fname;
  bool fcomment;
};

Flags parse_flags(char flags) {
  return Flags{
      (flags & 0b00000001) > 0, (flags & 0b00000010) > 0,
      (flags & 0b00000100) > 0, (flags & 0b00001000) > 0,
      (flags & 0b00010000) > 0,
  };
}
void dump_flags(char flags, Flags parsed_flags) {
  std::cout << "    " << std::bitset<3>((flags & 0b11100000) >> 5)
            << "      // reserved " << std::endl;
  std::cout << "    "
            << "   " << parsed_flags.fcomment << "    "
            << " // fcomment" << std::endl;
  std::cout << "    "
            << "    " << parsed_flags.fname << "   "
            << " // fname" << std::endl;
  std::cout << "    "
            << "     " << parsed_flags.fextra << "  "
            << " // fextra" << std::endl;
  std::cout << "    "
            << "      " << parsed_flags.fhcrc << " "
            << " // fhcrc" << std::endl;
  std::cout << "    "
            << "       " << parsed_flags.ftext << ""
            << " // ftext" << std::endl;
}

std::string read_string(std::ifstream &in) {
  auto bytes = read_until(in, '\0');
  std::string f(bytes.begin(), bytes.end());
  auto enc = std::string("Latin1");
  auto rsult = boost::locale::conv::to_utf<char>(f, enc);
  return rsult;
}

std::vector<char> read_until_eof(std::ifstream &in) {
  auto result = std::vector<char>(0);

  while (!in.eof()) {
    auto buffer = std::vector<char>(128);
    in.read(buffer.data(), 128);
    result.insert(result.end(), buffer.begin(), buffer.begin() + in.gcount());
  }
  return result;
}

uint32_t read_uint32(std::vector<char> bytes) {
  uint32_t n = 0;
  n |= (int)bytes[0];
  n |= (int)bytes[1] << 8;
  n |= (int)bytes[2] << 16;
  n |= (int)bytes[3] << 24;

  return n;
}

int main() {
  std::ifstream in("./raw.txt.gz", std::ios_base::in | std::ios_base::binary);

  auto header = read_n_bytes(in, 10);
  auto magic = std::vector(header.begin(), header.begin() + 2);
  if (magic != std::vector<char>{(char)0x1f, (char)0x8b}) {
    throw "oh no :(";
  }
  std::cout << "Header:\n";
  std::cout << "  Magic: ";
  dump_bytes(magic);

  auto compression_method = header.at(2);
  if (compression_method != 0x08) {
    throw "oh no :(";
  }
  std::cout << "  Compression Method: ";
  dump_compression_method(compression_method);

  auto flags = header.at(3);
  std::cout << "  Flags: " << std::endl;
  auto parsed_flags = parse_flags(flags);
  dump_flags(flags, parsed_flags);

  auto mtime = std::vector(header.begin() + 4, header.begin() + 8);
  auto m_time_number = read_uint32(mtime);
  std::cout << "  Modification Time: " << m_time_number << " ";
  dump_bytes(mtime);

  auto extra_flags = header.at(8);
  std::cout << "  Extra Flags: ";
  dump_bytes(std::vector{extra_flags});

  auto os = header.at(9);
  std::cout << "  OS: ";
  dump_bytes(std::vector{os});

  if (parsed_flags.fextra) {
    throw "oh no :(";
  }
  if (parsed_flags.fname) {
    auto rsult = read_string(in);
    std::cout << "  Filename: " << rsult << std::endl;
  }
  if (parsed_flags.fcomment) {
    auto rsult = read_string(in);
    std::cout << "  Comment: " << rsult << std::endl;
  }
  if (parsed_flags.fhcrc) {
    auto crc16 = read_n_bytes(in, 2);
    // todo: check header lol
    std::cout << "  CRC16: ";
    dump_bytes(crc16);
  }

  auto data_and_footer = read_until_eof(in);
  auto data = std::vector(data_and_footer.begin(), data_and_footer.end() - 8);
  auto footer = std::vector(data_and_footer.end() - 8, data_and_footer.end());
  std::cout << "Data: ";
  dump_bytes(data);

  std::cout << "Footer:\n";
  auto crc32 = std::vector(footer.begin(), footer.begin() + 4);
  std::cout << "  CRC32: ";
  dump_bytes(crc32);
  auto size_bytes = std::vector(footer.begin() + 4, footer.begin() + 8);
  dump_bytes(size_bytes);

  auto size = read_uint32(size_bytes);
  std::cout << "  Size: " << size << std::endl;
}
