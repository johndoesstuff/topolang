#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <stack>
#include <stdexcept>
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

	/*
	 * The tokenization process involves mapping the D-ary huffman encodings to
	 * their string or int counterparts.
     *
	 * A token is a single-sided chain of nested sets, e.g. {{{}, {}}, {}, {}}.
	 * Its "signature" is the size of each set along that chain, read from the
	 * innermost set outwards. Each size selects a child of the huffman tree
	 * ({{}} -> 1 -> λ, {{}, {}} -> 2 -> ℕ, {{}, {}, {}} -> 3 -> ops);
	 */
    SLC_set tokenize(const SLC_set &root) {
        SLC_element tok = tokenize_r(root);
        if (auto *s = std::get_if<std::shared_ptr<SLC_set>>(&tok)) {
            return **s;
		}
        // the whole topology was a single token
        SLC_set res;
        res.elements.insert(tok);
        return res;
    }

    // signature of a valid token, innermost set first
    std::vector<int> signature(const SLC_set &root) {
        std::vector<int> sig;
        const SLC_set *t = &root;
        while (t->elements.size() > 0) {
            sig.push_back(static_cast<int>(t->elements.size()));
            const SLC_set *next = nullptr;
            for (const auto &item : t->elements) {
                const auto &set_item = std::get<std::shared_ptr<SLC_set>>(item);
                if (set_item->elements.size() > 0)
                    next = set_item.get();
            }
            if (!next)
                break;
            t = next;
        }
        std::reverse(sig.begin(), sig.end());
        return sig;
    }

	// walks the huffman tree with the signature
    std::optional<SLC_element> walk(const std::vector<int> &sig) {
        const huffman_node *tree = &encoding_tree();
        for (size_t i = 0; i < sig.size(); i++) {
            int num = sig[i];
            bool last = i == sig.size() - 1;
            if (std::holds_alternative<huffman_node::node_list>(tree->data)) {
                const auto &vec = std::get<huffman_node::node_list>(tree->data);
                if (num < 1 || static_cast<size_t>(num) > vec.size())
                    return std::nullopt;
                tree = &vec[num - 1];
                // string leave dont consume any more sets
                if (std::holds_alternative<std::string>(tree->data)) {
                    if (!last)
                        return std::nullopt;
                    return SLC_element(
                        std::string_view(std::get<std::string>(tree->data)));
                }
            } else if (std::holds_alternative<natural_numbers>(tree->data)) {
                if (!last)
                    return std::nullopt;
                return SLC_element(std::get<natural_numbers>(tree->data)[num]);
            }
        }
        return std::nullopt;
    }

    SLC_element tokenize_r(const SLC_set &root) {
        if (valid_token(root)) {
            if (auto tok = walk(signature(root)))
                return *tok;
        }
        // try to tokenize the children instead :P
        SLC_set res;
        for (const auto &item : root.elements) {
            const auto &set_item = std::get<std::shared_ptr<SLC_set>>(item);
            res.elements.insert(tokenize_r(*set_item));
        }
        return std::make_shared<SLC_set>(std::move(res));
    }
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

    std::cout << parsed.to_string() << std::endl;
    std::cout << parser.tokenize(parsed).to_string() << std::endl;
}
