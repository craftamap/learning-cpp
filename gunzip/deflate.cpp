#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <sys/types.h>
#include <utility>
#include <vector>

class Reader {

public:
  Reader(std::vector<char> &by) : m_bytes(by) {}

  char read_bit() {
    // std::cout << (int)m_bytes.at(m_byte_pos);
    // std::cout << "byte_pos" << m_byte_pos << "\n";
    // std::cout << "bit_mask" << (int)m_bit_mask << "\n";
    auto bit = (m_bytes.at(m_byte_pos) & m_bit_mask) ? 1 : 0;
    m_bit_mask <<= 1;
    if (!m_bit_mask) {
      m_bit_mask = 1;
      m_byte_pos += 1;
    }
    // std::cout << "bit" << (int)bit << "\n";
    return bit;
  }

  int read_bits_inv(int count) {
    int value = 0;
    for (int i = 0; i < count; i++) {
      auto bit = read_bit();
      value = value | (bit << i);
    }
    return value;
  }

  bool is_eof() {
    return m_byte_pos == m_bytes.size() - 1 && m_bit_mask == -128;
  }

private:
  uint64_t m_byte_pos = 0;
  char m_bit_mask = 1;
  std::vector<char> &m_bytes;
};

class HuffmanNode {

public:
  int m_code;
  std::shared_ptr<HuffmanNode> m_zero;
  std::shared_ptr<HuffmanNode> m_one;

  HuffmanNode(int code)
      : m_code(code), m_zero(std::move(std::shared_ptr<HuffmanNode>(nullptr))),
        m_one(std::shared_ptr<HuffmanNode>(nullptr)) {}

  int read(Reader &reader) {
    auto current_node = this;
    while (current_node->m_code == -1) {
      auto bit = reader.read_bit();
      if (bit) {
        current_node = current_node->m_one.get(); // this is awful
      } else {
        current_node = current_node->m_zero.get();
      }
    }
    return current_node->m_code;
  }

  void print(std::string prefix) {
    if (m_code == -1) {
      if (m_zero) {
        m_zero->print(prefix + "0");
      }
      if (m_one) {
        m_one->print(prefix + "1");
      }
    } else {
      std::cout << prefix << " " << m_code << std::endl;
    }
  }
};

struct HuffmanRange {
  int end;
  int bit_length;
};

struct TreeNode {
  uint32_t len;
  uint32_t code;
};

std::shared_ptr<HuffmanNode>
build_huffman_tree(std::vector<HuffmanRange> ranges) {
  int max_bit_length = 0;
  for (int n = 0; n < ranges.size(); n++) {
    if (ranges[n].bit_length > max_bit_length) {
      max_bit_length = ranges[n].bit_length;
    }
  };

  auto bl_count = std::vector<int>(max_bit_length + 1, 0);
  auto next_code = std::vector<int>(max_bit_length + 1, 0);
  auto tree = std::vector<TreeNode>(ranges.back().end + 1);

  for (int n = 0; n < ranges.size(); n++) {
    bl_count[ranges[n].bit_length] +=
        ranges[n].end - ((n > 0) ? ranges[n - 1].end : -1);
  };

  // step 2
  int code = 0;
  for (int bits = 1; bits <= max_bit_length; bits++) {
    code = (code + bl_count[bits - 1]) << 1;
    if (bl_count[bits]) {
      next_code[bits] = code;
    }
  }
  // step 3
  int active_range = 0;
  for (int n = 0; n <= ranges.back().end; n++) {
    if (n > ranges[active_range].end) {
      active_range++;
    }
    if (ranges[active_range].bit_length) {
      auto current_bit_length = ranges[active_range].bit_length;
      tree[n].len = current_bit_length;
      if (tree[n].len != 0) {
        tree[n].code = next_code[current_bit_length];
        next_code[current_bit_length]++;
      }
    }
  }

  auto root = std::make_shared<HuffmanNode>(HuffmanNode(-1));

  std::shared_ptr<HuffmanNode> node = root;
  for (int n = 0; n <= ranges.back().end; n++) {
    std::shared_ptr<HuffmanNode> node = root;
    if (tree[n].len) {
      for (int bits = tree[n].len; bits; bits--) {
        if (tree[n].code & (1 << (bits - 1))) {
          if (!node->m_one) {
            node->m_one = std::make_shared<HuffmanNode>(-1);
          }
          node = node->m_one;
        } else {
          if (!node->m_zero) {
            node->m_zero = std::make_shared<HuffmanNode>(-1);
          }
          node = node->m_zero;
        }
      }
      node->m_code = n;
    }
  }

  return root;
}

const int DYNAMIC_HUFFMAN = 2;

void inflate(Reader &reader, std::shared_ptr<HuffmanNode> literals_root,
             std::shared_ptr<HuffmanNode> distances_root) {
  int extra_length_addend[] = {11, 13, 15, 17, 19, 23,  27,  31,  35,  43,
                               51, 59, 67, 83, 99, 115, 131, 163, 195, 227};
  int extra_dist_addend[] = {4,    6,    8,     12,    16,   24,   32,
                             48,   64,   96,    128,   192,  256,  384,
                             512,  768,  1024,  1536,  2048, 3072, 4096,
                             6144, 8192, 12288, 16384, 24576};

  std::vector<char> buffer(0);
  auto stop_code = false;
  while (!stop_code) {
    auto code = literals_root->read(reader);
    // found a leaf
    if (code == 256) {
      stop_code = true;
      break;
    }

    if (code < 256) {
      auto literal_byte = code;
      // std::cout << "literal: " << (char)literal_byte << "\n";
      buffer.push_back(literal_byte);
    } else if (code > 256) {
      int length;
      int dist;
      int extra_bits;
      if (code < 265) {
        length = code - 254;
        // std::cout << "length direct" << length << "\n";
      } else {
        if (code < 285) {
          // std::cout << "code " << code << " length"
          //           << (code - 261) / 4 << "\n";
          extra_bits = reader.read_bits_inv((code - 261) / 4);
          // std::cout << "extra bits" << extra_bits << "\n";
          length = extra_bits + extra_length_addend[code - 265];
          // std::cout << "length calculated" << length << "\n";
        } else {
          length = 258;
        }
      }
      dist = distances_root->read(reader);
      if (dist > 3) {
        auto extra_dist = reader.read_bits_inv((dist - 2) / 2);
        // std::cout << "dist " << dist << " extra dist" << (int) extra_dist
        // << "\n";
        auto addend = extra_dist_addend[dist - 4];
        dist = extra_dist + addend;
        // std::cout << "final dist" << dist << "addend" << addend << "\n";
      }

      // do we need the -1?
      auto backptr = buffer.end() - dist - 1;
      while (length--) {
        // std::cout << "backptr: " << (char)*backptr << "\n";
        buffer.push_back(*backptr++);
      }
    }
  }
  std::string bytes(buffer.begin(), buffer.end());
  std::cout << "actual bytes: " << bytes << "\n";
}

std::vector<char> dynamic_huffman(Reader &reader) {
  int hlit = reader.read_bits_inv(5);
  int code_lengths_literals = hlit + 257;
  int hdist = reader.read_bits_inv(5);
  int code_lengths_distance_alpha = hdist + 1;
  int hclen = reader.read_bits_inv(4);
  int code_lengths_for_code_lengths_3bits = (hclen + 4);
  std::cout << "  Dynamic Huffman: " << std::endl;
  std::cout << "    HLIT: " << hlit << " code lengths for literal "
            << code_lengths_literals << std::endl;
  std::cout << "    HDIST: " << hdist << " code_lengths_distance_alpha "
            << code_lengths_distance_alpha << std::endl;
  std::cout << "    HCLEN: " << hclen << " code_lengths_for_code_length "
            << code_lengths_for_code_lengths_3bits * 3 << std::endl;

  std::vector<int> code_lengths(19);
  int code_length_offsets[] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                               11, 4,  12, 3, 13, 2, 14, 1, 15};
  for (int i = 0; i < code_lengths_for_code_lengths_3bits; i++) {
    auto a = reader.read_bits_inv(3);
    std::cout << "   " << (int)a << std::endl;
    code_lengths[code_length_offsets[i]] = ((int)a);
  }

  auto code_length_ranges = std::vector<HuffmanRange>(19);

  int j = 0;
  for (int i = 0; i < 19; i++) {
    if ((i > 0) && (code_lengths[i] != code_lengths[i - 1])) {
      j++;
    }
    code_length_ranges[j].end = i;
    code_length_ranges[j].bit_length = code_lengths[i];
  }

  auto codelengths_huffmann_tree = build_huffman_tree(std::vector(
      code_length_ranges.begin(), code_length_ranges.begin() + j + 1));

  std::vector<int> alphabet(code_lengths_literals + code_lengths_distance_alpha,
                            0);

  int i = 0;
  auto current_node = codelengths_huffmann_tree;
  while (i < code_lengths_literals + code_lengths_distance_alpha) {
    auto bit = reader.read_bit();
    if (bit) {
      current_node = current_node->m_one;
    } else {
      current_node = current_node->m_zero;
    }
    if (current_node->m_code != -1) {
      std::cout << "code: " << current_node->m_code << "\n";
      if (current_node->m_code > 15) {
        int repeat_length;
        switch (current_node->m_code) {
        case 16:
          repeat_length = reader.read_bits_inv(2) + 3;
          break;
        case 17:
          repeat_length = reader.read_bits_inv(3) + 3;
          break;
        case 18:
          repeat_length = reader.read_bits_inv(7) + 11;
          break;
        }
        while (repeat_length--) {
          if (current_node->m_code == 16) {
            alphabet[i] = alphabet[i - 1];
          } else {
            alphabet[i] = 0;
          }
          i++;
        }
      } else {
        alphabet[i] = current_node->m_code;
        i++;
      }
      current_node = codelengths_huffmann_tree;
    }
  }
  for (auto x : alphabet) {
    std::cout << x << "\n";
  }
  std::vector<HuffmanRange> alphabet_ranges(code_lengths_literals +
                                            code_lengths_distance_alpha);

  auto j2 = 0;
  for (auto i = 0; i <= code_lengths_literals; i++) {
    if (i > 0 && alphabet[i] != alphabet[i - 1]) {
      j2++;
    }
    alphabet_ranges[j2].end = i;
    alphabet_ranges[j2].bit_length = alphabet[i];
  }

  auto literals_root = build_huffman_tree(
      std::vector(alphabet_ranges.begin(), alphabet_ranges.begin() + j2));

  auto j3 = 0;
  for (auto i = code_lengths_literals - 1;
       i <= (code_lengths_literals + code_lengths_distance_alpha); i++) {
    if (i > (code_lengths_literals) && alphabet[i] != alphabet[i - 1]) {
      j3++;
    }
    alphabet_ranges[j3].end = i - code_lengths_literals;
    alphabet_ranges[j3].bit_length = alphabet[i];
  }

  auto distances_root = build_huffman_tree(
      std::vector(alphabet_ranges.begin(), alphabet_ranges.begin() + j3));

  inflate(reader, literals_root, distances_root);

  return std::vector<char>{};
}

struct ReadBlockResult {
  std::vector<char> bytes;
  bool final;
};

ReadBlockResult read_block(Reader &reader) {
  char final = reader.read_bits_inv(1);
  char type = reader.read_bits_inv(2);

  std::cout << "Block:" << std::endl;
  std::cout << "  BFINAL " << (int) final << std::endl;
  std::cout << "  BTYPE " << (int)type << std::endl;

  if (type == DYNAMIC_HUFFMAN) {
    dynamic_huffman(reader);
  } else {
    throw "oh no ";
  }

  return ReadBlockResult{
      .bytes = std::vector<char>{},
      .final = (bool) final,
  };
}

std::vector<char> deflate(std::vector<char> bytes) {
  auto reader = Reader(bytes);
  auto final = false;
  while (!final) {
    auto result = read_block(reader);
    final = result.final;
  }

  return std::vector<char>{};
}
