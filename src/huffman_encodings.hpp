#include <memory>
#include <string>
#include <variant>
#include <vector>

// wow! ℕ[n] -> n! genius abstraction!
// index numbers are
struct index_numbers {
    int operator[](size_t index) const noexcept {
        return static_cast<int>(index);
    }
};

struct literal_numbers {
    int operator[](size_t value) const noexcept {
        return static_cast<int>(value);
    }
};

struct huffman_node {
    using node_list = std::vector<huffman_node>;

    using huffman_value =
        std::variant<std::string, index_numbers, literal_numbers, node_list>;

    huffman_value data;

    huffman_node(std::string s) : data(std::move(s)) {}
    huffman_node(index_numbers n) : data(n) {}
    huffman_node(literal_numbers n) : data(n) {}
    huffman_node(node_list children) : data(std::move(children)) {}
};

inline const huffman_node &encoding_tree() {
    static const huffman_node tree{huffman_node::node_list{
        huffman_node("λ"),
        huffman_node(index_numbers{}), // first natural numbers are for debruijn
        huffman_node(
            huffman_node::node_list{huffman_node("+"), huffman_node("-"),
                                    huffman_node("*"), huffman_node("/")}),
        huffman_node(
            literal_numbers{})}}; // second natural numbers are int literals

    return tree;
}
