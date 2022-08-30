// Huffman coding
// https://en.wikipedia.org/wiki/Huffman_coding

#ifndef HUFFMAN_H
#define HUFFMAN_H
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Huffman {
namespace detail {
// Easily serializable:
// Trivial type with well-defined size of all fields.
struct Node {
  std::int16_t left;   // Left Node in tree. -1 means 'none'
  std::int16_t right;  // Right Node in tree. -1 means 'none'
  std::uint8_t value;  // Char value but always unsigned

  [[nodiscard]] constexpr bool isLeaf() const noexcept {
	return (left == -1) && (right == -1);
  }
};
}  // namespace detail

// This class represents fully compressed text
struct Encoded {
 private:
  std::vector<bool> binary_data_;     // bitset to store data compactly
  std::vector<detail::Node> nodes_;   // the tree

  void init_tree(std::span<std::uint8_t const> input_data);
  void init_binary_data(std::span<std::uint8_t const> input_data);

  // Root element is always the last one.
  // There is _always_ at least 1 element.
  [[nodiscard]] auto const &root() const {
	return nodes_.back();
  }
  [[nodiscard]] auto root_index() const {
	return static_cast<int16_t>(nodes_.size() - 1);
  }

 public:
  Encoded() = default;

  static Encoded encode(std::span<std::uint8_t const> input_data);
  static Encoded encode(void const *source, std::size_t size);
  static Encoded encode(std::span<std::byte const> input_data);
  static Encoded encode(std::string_view input_data);

  [[nodiscard]] std::string decode() const;
};

static_assert(CHAR_BIT == 8, "8 bit byte is assumed");
}  // namespace Huffman

#endif  // HUFFMAN_H