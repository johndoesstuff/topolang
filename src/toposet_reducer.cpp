#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <stack>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#endif

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
                // string leaves don't consume any more sets
                if (std::holds_alternative<std::string>(tree->data)) {
                    if (!last)
                        return std::nullopt;
                    std::string_view name = std::get<std::string>(tree->data);
                    if (name == "λ")
                        return SLC_element(SLC_atom::lambda());
                    return SLC_element(SLC_atom::op(name));
                }
            } else if (std::holds_alternative<index_numbers>(tree->data)) {
                if (!last)
                    return std::nullopt;
                return SLC_element(
                    SLC_atom::index(std::get<index_numbers>(tree->data)[num]));
            } else if (std::holds_alternative<literal_numbers>(tree->data)) {
                if (!last)
                    return std::nullopt;
                return SLC_element(SLC_atom::integer(
                    std::get<literal_numbers>(tree->data)[num]));
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

    static bool is_set(const SLC_element &e) {
        return std::holds_alternative<std::shared_ptr<SLC_set>>(e);
    }
    static const SLC_set &as_set(const SLC_element &e) {
        return *std::get<std::shared_ptr<SLC_set>>(e);
    }

    // build a single-sided chain from a signature (innermost set first)
    static SLC_element build_chain(const std::vector<int> &sig) {
        SLC_element cur = std::make_shared<SLC_set>();
        for (int size : sig) {
            SLC_set s;
            s.elements.insert(cur);
            for (int i = 1; i < size; i++)
                s.elements.insert(std::make_shared<SLC_set>());
            cur = std::make_shared<SLC_set>(std::move(s));
        }
        return cur;
    }

    // find the signature of a string token by searching the huffman tree
    static bool find_path(const huffman_node &tree, std::string_view name,
                          std::vector<int> &sig) {
        if (std::holds_alternative<std::string>(tree.data))
            return std::get<std::string>(tree.data) == name;
        if (!std::holds_alternative<huffman_node::node_list>(tree.data))
            return false;
        const auto &vec = std::get<huffman_node::node_list>(tree.data);
        for (size_t i = 0; i < vec.size(); i++) {
            if (find_path(vec[i], name, sig)) {
                sig.push_back(static_cast<int>(i) + 1);
                return true;
            }
        }
        return false;
    }

    // finds the root child holding number leaves of the given kind
    template <typename NumberKind> static int number_child() {
        const auto &root =
            std::get<huffman_node::node_list>(encoding_tree().data);
        for (size_t i = 0; i < root.size(); i++)
            if (std::holds_alternative<NumberKind>(root[i].data))
                return static_cast<int>(i) + 1;
        throw std::runtime_error("Number kind missing from encoding tree");
    }

    static SLC_element detokenize_r(const SLC_element &e) {
        if (const auto *atom = std::get_if<SLC_atom>(&e)) {
            if (atom->type == SLC_atom_type::INDEX)
                return build_chain({number_child<index_numbers>(), atom->ival});
            if (atom->type == SLC_atom_type::INTEGER)
                return build_chain(
                    {number_child<literal_numbers>(), atom->ival});
            std::string_view name =
                atom->type == SLC_atom_type::LAMBDA ? "λ" : atom->sval;
            std::vector<int> sig;
            if (!find_path(encoding_tree(), name, sig))
                throw std::runtime_error("Unknown token");
            // find_path collects leaf-to-root; the chain wants the root's
            // choice innermost
            std::reverse(sig.begin(), sig.end());
            return build_chain(sig);
        }
        SLC_set res;
        for (const auto &item : as_set(e).elements)
            res.elements.insert(detokenize_r(item));
        return std::make_shared<SLC_set>(std::move(res));
    }

    SLC_set detokenize(const SLC_set &root) {
        return as_set(detokenize_r(std::make_shared<SLC_set>(root)));
    }

    /*
     * Reduction works on tokenized toposets, containing De Bruijn terms:
     *   variable     n
     *   abstraction  {λ, body}
     *   application  {{f}, x}
     *
     * A term is either an int or a 2-element set, so {f} is always
     * distinguishable from a term and encoding is unambiguous
     */
    enum class term_kind { VAR, LAM, APP, UNKNOWN };

    struct term_view {
        term_kind kind = term_kind::UNKNOWN;
        int var = 0;
        SLC_element a; // LAM: body, APP: function
        SLC_element b; // APP: argument
    };

    static SLC_element make_set(std::initializer_list<SLC_element> elems) {
        SLC_set s;
        for (const auto &e : elems)
            s.elements.insert(e);
        return std::make_shared<SLC_set>(std::move(s));
    }

    static term_view view(const SLC_element &e) {
        term_view v;
        if (const auto *atom = std::get_if<SLC_atom>(&e)) {
            if (atom->type == SLC_atom_type::INDEX) {
                v.kind = term_kind::VAR;
                v.var = atom->ival;
            }
            // operators dont do anything for now.. soon..
            return v;
        }
        if (!is_set(e) || as_set(e).elements.size() != 2)
            return v;
        const auto &elems = as_set(e).elements;
        std::optional<SLC_element> lam, fn, other;
        for (const auto &item : elems) {
            if (const auto *atom = std::get_if<SLC_atom>(&item);
                atom && atom->type == SLC_atom_type::LAMBDA)
                lam = item;
            else if (is_set(item) && as_set(item).elements.size() == 1)
                fn = *as_set(item).elements.begin();
            else
                other = item;
        }
        if (lam && !fn && other) {
            v.kind = term_kind::LAM;
            v.a = *other;
        } else if (fn && !lam) {
            // the argument may itself be a singleton-free term, or the second
            // element might also be a singleton (never the case for terms
            // produced by tokenize, since terms are ints or 2-element sets)
            v.kind = term_kind::APP;
            v.a = *fn;
            v.b = other ? *other : SLC_element(*fn);
        }
        return v;
    }

    // add `amount` to every free variable >= cutoff
    static SLC_element shift(const SLC_element &e, int amount, int cutoff) {
        term_view v = view(e);
        switch (v.kind) {
        case term_kind::VAR:
            return v.var >= cutoff
                       ? SLC_element(SLC_atom::index(v.var + amount))
                       : e;
        case term_kind::LAM:
            return make_set({SLC_element(SLC_atom::lambda()),
                             shift(v.a, amount, cutoff + 1)});
        case term_kind::APP:
            return make_set({make_set({shift(v.a, amount, cutoff)}),
                             shift(v.b, amount, cutoff)});
        default:
            return e;
        }
    }

    static SLC_element substitute(const SLC_element &e, const SLC_element &arg,
                                  int depth) {
        term_view v = view(e);
        switch (v.kind) {
        case term_kind::VAR:
            if (v.var == depth)
                return shift(arg, depth - 1, 1);
            if (v.var > depth)
                return SLC_element(SLC_atom::index(v.var - 1));
            return e;
        case term_kind::LAM:
            return make_set({SLC_element(SLC_atom::lambda()),
                             substitute(v.a, arg, depth + 1)});
        case term_kind::APP:
            return make_set({make_set({substitute(v.a, arg, depth)}),
                             substitute(v.b, arg, depth)});
        default:
            return e;
        }
    }

    // reduce the leftmost-outermost redex (normal order). sets `changed` if
    // a redex was found
    static SLC_element reduce_r(const SLC_element &e, bool &changed) {
        term_view v = view(e);
        switch (v.kind) {
        case term_kind::LAM: {
            SLC_element body = reduce_r(v.a, changed);
            return changed ? make_set({SLC_element(SLC_atom::lambda()), body})
                           : e;
        }
        case term_kind::APP: {
            term_view f = view(v.a);
            if (f.kind == term_kind::LAM) {
                changed = true;
                return substitute(f.a, v.b, 1);
            }
            SLC_element fn = reduce_r(v.a, changed);
            if (changed)
                return make_set({make_set({fn}), v.b});
            SLC_element arg = reduce_r(v.b, changed);
            if (changed)
                return make_set({make_set({v.a}), arg});
            return e;
        }
        default:
            return e;
        }
    }

    // perform 1 step of reduction if not already in reduced form
    SLC_set reduce(const SLC_set &root, bool *changed = nullptr) {
        bool did = false;
        SLC_element res = reduce_r(std::make_shared<SLC_set>(root), did);
        if (changed)
            *changed = did;
        if (auto *s = std::get_if<std::shared_ptr<SLC_set>>(&res))
            return **s;
        SLC_set wrapped;
        wrapped.elements.insert(res);
        return wrapped;
    }

    // reduce until normal form (or until max_steps)
    SLC_set normalize(const SLC_set &root, int max_steps = 10000) {
        SLC_set cur = root;
        for (int i = 0; i < max_steps; i++) {
            bool changed = false;
            cur = reduce(cur, &changed);
            if (!changed)
                break;
        }
        return cur;
    }
};

int main() {
    toposet_parser parser(
        "{{{}}, {{{}}, {{{{{{{}, {}}, {}}}, {{{}}, {{{}}, {{{}}, {{{{{}, {}}}, "
        "{{{{{}, {{}, {}}, {}}}, {{}, {{}, {}}}}}}, {{{{}, {}}, {}}}}}}}}}, "
        "{{{}, {}}}}}}");

    auto parsed = parser.parse_toposet();

    std::cout << parsed.to_string() << std::endl;
    auto tokenized = parser.tokenize(parsed);
    std::cout << tokenized.to_string() << std::endl;
    std::cout << parser.normalize(tokenized).to_string() << std::endl;
}

std::string slc2dbj(std::string str) {
    toposet_parser parser(str);
    return parser.tokenize(parser.parse_toposet()).to_string();
}

std::string reduce_slc(std::string str) {
    toposet_parser parser(str);
    auto tokenized = parser.tokenize(parser.parse_toposet());
    return parser.detokenize(parser.reduce(tokenized)).to_string();
}

// emscripten bindings

#ifdef __EMSCRIPTEN__

EMSCRIPTEN_BINDINGS(toposet_reducer) {
    emscripten::function("slc2dbj", &slc2dbj);
    emscripten::function("reduce_slc", &reduce_slc);
}

#endif
