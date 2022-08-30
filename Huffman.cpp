#include "Huffman.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <execution>
#include <queue>
#include <span>

using Huffman::detail::Node, Huffman::Encoded, std::int16_t, std::uint8_t, std::size_t;

// We need the counts while encoding, but not later one.
struct NodeWithCount {
  Node node;
  size_t count;
};

// Given a range of bytes, return the count of each unique value.
constexpr auto count_bytes(std::span<uint8_t const> const bytes) noexcept {
  std::array<size_t, UCHAR_MAX + 1> counts = {};

  for (auto const ch: bytes) {
	++counts[ch];
  }

  return counts;
}

// Given the number of each unique byte value, form the "leaf" nodes of our tree
static auto make_leaf_nodes(std::span<size_t const> const counts) {
  std::vector<NodeWithCount> nodes;

  for (size_t i = 0, sz = counts.size(); i < sz; ++i) {
	if (counts[i]) {
	  nodes.push_back(NodeWithCount{Node{-1, -1, static_cast<uint8_t>(i)}, counts[i]});
	}
  }

  return nodes;
}

// This type is intended to store the "path" to each character
// in our tree. Used to "encode" the data initially.
// '\0' -> left
// '\1' -> right
using CharDictT = std::array<std::string, UCHAR_MAX + 1>;

static auto init_dict(std::vector<Node> const &nodes, int16_t root,
					  CharDictT &dict, std::string &key) {
  if (root < 0) {
	return;
  }
  assert(root < nodes.size());

  auto const &elm = nodes[root];

  if (elm.isLeaf()) {
	dict[elm.value] = key;
	return;
  }

  key.push_back(0);
  init_dict(nodes, elm.left, dict, key);
  key.back() = 1;
  init_dict(nodes, elm.right, dict, key);
  key.pop_back();
}

std::string Huffman::Encoded::decode() const {
  // https://en.wikipedia.org/wiki/Huffman_coding#Decompression

  auto node = root();
  std::string str;

  for (bool i: binary_data_) {
	auto const go_right = i;
	node = nodes_[go_right ? node.right : node.left];
	if (node.isLeaf()) {
	  str += static_cast<char>(node.value);
	  node = root();
	}
  }

  return str;
}

Encoded Encoded::encode(std::span<std::uint8_t const> input_data) {
  // https://en.wikipedia.org/wiki/Huffman_coding#Compression

  Encoded encoded;
  encoded.init_tree(input_data);
  encoded.init_binary_data(input_data);
  return encoded;
}

void Encoded::init_tree(std::span<std::uint8_t const> const input_data) {
  auto nodes_with_size = make_leaf_nodes(count_bytes(input_data));

  auto const cmp = [&nodes_with_size](int16_t const lhs,
									  int16_t const rhs) noexcept -> bool {
	// reverse compare
	return nodes_with_size[lhs].count > nodes_with_size[rhs].count;
  };

  auto queue =
	  std::priority_queue<int16_t, std::vector<int16_t>, decltype(cmp)>(cmp);

  for (int16_t i = 0, sz = static_cast<int16_t>(nodes_with_size.size());
	   i < sz; ++i) {
	if (nodes_with_size[i].count) {
	  queue.push(i);
	}
  }

  // 0 and 1 are trivial cases that the later while loop does not handle.
  switch (queue.size()) {
	case 0: nodes_ = {Node{-1, -1, 0}};
	  return;
	case 1: nodes_ = {Node{-1, -1, input_data.front()}, Node{-1, 0, 0}};
	  return;
	default: break;
  }

  while (queue.size() > 1) {
	auto back1 = queue.top();
	queue.pop();
	auto back2 = queue.top();
	queue.pop();
	nodes_with_size.emplace_back(
		Node{back1, back2, 0},
		nodes_with_size[back1].count + nodes_with_size[back2].count);
	queue.push(static_cast<int16_t>(nodes_with_size.size() - 1));
  }

  nodes_.resize(nodes_with_size.size());

  std::transform(nodes_with_size.begin(), nodes_with_size.end(),
				 nodes_.begin(), [](NodeWithCount const &withCount) -> auto & { return withCount.node; });
}

static auto init_dict(std::vector<Node> const &nodes, int16_t root,
					  CharDictT &dict) {
  std::string key;
  init_dict(nodes, root, dict, key);
}

void Huffman::Encoded::init_binary_data(std::span<std::uint8_t const> const input_data) {
  // Step 1: Create a dictionary with paths for all characters

  CharDictT dict = {};

  init_dict(nodes_, root_index(), dict);

  // Step 2: Calculate the final length of the binary data

  // This binary operator handles out-of-sequence sums for std::reduce
  struct BinOp {
	// The dictionary is just a pointer, because functors are often copied in C++,
	// and we don't want to copy 256 std::strings left and right.
	CharDictT *dict;

	[[nodiscard]] auto operator()(size_t const a, size_t const b) const noexcept { return a + b; }
	[[nodiscard]] auto operator()(size_t const a, uint8_t const b) const noexcept {
	  return a + (*dict)[b].size();
	}
	[[nodiscard]] auto operator()(uint8_t const a, size_t const b) const noexcept {
	  return (*dict)[a].size() + b;
	}
	[[nodiscard]] auto operator()(uint8_t const a, uint8_t const b) const noexcept {
	  return (*dict)[a].size() + (*dict)[b].size();
	}
  };

  auto const size = std::reduce(std::execution::par_unseq, input_data.begin(),
								input_data.end(), size_t{}, BinOp{&dict});

  binary_data_.resize(size);

  // Step 3: Concat all paths

  size_t bit_iter = 0;
  for (uint8_t ch: input_data) {
	for (auto const &bit: dict[ch]) {
	  binary_data_[bit_iter++] = !!bit;
	}
  }
}

Encoded Encoded::encode(void const *const source, std::size_t const size) {
  return encode(std::span{static_cast<std::uint8_t const *>(source), size});
}

Encoded Encoded::encode(std::span<std::byte const> const input_data) {
  return encode(input_data.data(), input_data.size());
}

Encoded Encoded::encode(std::string_view const input_data) {
  return encode(input_data.data(), input_data.size());
}