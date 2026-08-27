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

    // only accepts untokenized toposets
    bool valid_token(const SLC_set &s) {
        int rec = 0;
        for (auto item : s.elements) {
            auto set_item = std::get<std::shared_ptr<SLC_set>>(item);
            if (set_item->elements.size() > 0) {
                rec++;
                if (!valid_token(*set_item.get())) {
                    return false;
                }
            }
        }
        if (rec <= 1)
            return true;
        return false;
    }

    // the tokenization process of a topology involves mapping the D-ary
    // huffman encodings to their string or int counterparts
    SLC_set tokenize(SLC_set &root) {
        if (!valid_token(root)) {
            for (auto &item : root.elements) {
                auto &set_item = std::get<std::shared_ptr<SLC_set>>(item);
                tokenize(*set_item.get());
            }
        } else {
            // determine if valid huffman walk exists for token: first we must
            // get its "signature" as a vector
            std::vector<int> sig;
            auto t = root;
            while (t.elements.size() > 0) {
                sig.push_back(static_cast<int>(t.elements.size()));
                auto current = t;
                t = *std::get<std::shared_ptr<SLC_set>>(*(t.elements.begin()));
                for (auto item : current.elements) {
                    auto set_item = std::get<std::shared_ptr<SLC_set>>(item);
                    if (set_item->elements.size() > 0) {
                        t = *set_item;
                    }
                }
            }
            std::reverse(sig.begin(), sig.end());
		}
        return root;
    }
    /*SLC_set tokenize(SLC_set root) {
        SLC_set res;
        res.elements.insert(tokenize_r(root));
        return res;
    }
    SLC_element tokenize_r(SLC_set root) {
        // determine is set is valid to be a token (single sided recursion)
        bool valid = valid_token(root);
        if (valid) {
            // determine if valid huffman walk exists for token: first we must
            // get its "signature" as a vector
            std::vector<int> sig;
            auto t = root;
            while (t.elements.size() > 0) {
                sig.push_back(static_cast<int>(t.elements.size()));
                auto current = t;
                t = *std::get<std::shared_ptr<SLC_set>>(*(t.elements.begin()));
                for (auto item : current.elements) {
                    auto set_item = std::get<std::shared_ptr<SLC_set>>(item);
                    if (set_item->elements.size() > 0) {
                        t = *set_item;
                    }
                }
            }
            std::reverse(sig.begin(), sig.end());
            // determine if signature is a valid walk of the huffman tree
            auto tree = encoding_tree();
            SLC_element result;
            for (int i = 0; i < sig.size(); i++) {
                int num = sig[i];
                if (std::holds_alternative<std::vector<huffman_node>>(
                        tree.data) &&
                    valid) {
                    auto vec = std::get<std::vector<huffman_node>>(tree.data);
                    if (num < vec.size()) {
                        tree = vec[num];
                    } else {
                        valid = false;
                    }
                } else if (std::holds_alternative<natural_numbers>(tree.data) &&
                           i == sig.size() - 1 && valid) {
                    return std::get<natural_numbers>(tree.data)[num];
                } else if (std::holds_alternative<std::string>(tree.data) &&
                           i == sig.size() - 1 && valid) {
                    return std::get<std::string>(tree.data);
                }
            }
        }
        for (auto item : root.elements) {
            auto set_item = std::get<std::shared_ptr<SLC_set>>(item);
            item = tokenize_r(*set_item.get());
        }
        return root;
    }*/
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

    auto parsed = parser.parse_toposet();

    std::cout << parsed.to_string();
    std::cout << parser.tokenize(parsed).to_string();
}
