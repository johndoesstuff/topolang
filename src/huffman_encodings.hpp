#include <memory>
#include <string>
#include <variant>
#include <vector>

// wow! ℕ[n] -> n! genius abstraction!
struct natural_numbers {
    int operator[](size_t index) const noexcept {
        return static_cast<int>(index);
    }
};

struct huffman_node {
    using node_list = std::vector<huffman_node>;

    using huffman_value = std::variant<std::string, natural_numbers, node_list>;

    huffman_value data;

    huffman_node(std::string s) : data(std::move(s)) {}
    huffman_node(natural_numbers n) : data(n) {}
    huffman_node(node_list children) : data(std::move(children)) {}
};

inline const huffman_node &encoding_tree() {
    static const huffman_node tree{huffman_node::node_list{
        huffman_node("λ"), huffman_node(natural_numbers{}),
        huffman_node(
            huffman_node::node_list{huffman_node("+"), huffman_node("-"),
                                    huffman_node("*"), huffman_node("/")})}};

    return tree;
}
