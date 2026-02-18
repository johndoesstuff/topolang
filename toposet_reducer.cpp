#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <variant>
#include <vector>

#include "huffman_encodings.hpp"
#include "slc_set.hpp"

struct toposet_parser {
    std::string_view str_;
    toposet_parser(std::string_view str) : str_(str) {}

    SLC_set parse_toposet() {
        std::stack<SLC_set> stk;
        SLC_set base;

        for (char ch : str_) {
            if (ch == '{') {
                SLC_set next;
                stk.push(next);
            } else if (ch == '}') {
                if (stk.empty())
                    throw std::runtime_error("Unexpected '}'");

                SLC_set completed_set = std::move(stk.top());
                stk.pop();

                if (!stk.empty()) {
                    stk.top().elements.insert(
                        std::make_shared<SLC_set>(std::move(completed_set)));
                } else {
                    return completed_set;
                }
            }
        }
        throw std::runtime_error("Could not parse!");
        return base;
    }

    // the tokenization process of a topology involves mapping the D-ary
    // huffman encodings to their string or int counterparts
    SLC_set tokenize() {}
};

int main() {
    std::vector<huffman_node> ops;
    ops.emplace_back("+");
    ops.emplace_back("-");
    ops.emplace_back("*");
    ops.emplace_back("/");

    std::vector<huffman_node> root_children;
    root_children.emplace_back("λ");
    root_children.emplace_back(natural_numbers());
    root_children.emplace_back(std::move(ops));

    huffman_node huffman_tree(std::move(root_children));

    toposet_parser parser(
        "{{{}}, {{{}}, {{{{{{{}, {}}, {}}}, {{{}}, {{{}}, {{{}}, {{{{{}, {}}}, "
        "{{{{{}, {{}, {}}, {}}}, {{}, {{}, {}}}}}}, {{{{}, {}}, {}}}}}}}}}, "
        "{{{}, {}}}}}}");
    std::cout << parser.parse_toposet().to_string();
}
