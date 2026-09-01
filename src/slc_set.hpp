#pragma once

#include <algorithm>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct SLC_set;
struct SLC_atom;

enum class SLC_atom_type {
    LAMBDA,   // λ
    INDEX,    // used for debruijn
    OPERATOR, // + - * etc..
    INTEGER,  // int literal for operators
};

// binary (and even ternary) ops require multiple steps to reduce due to the
// nature of ulc, therefore we must keep track of what step in the reduction
// process each op is to evaluate it
struct SLC_op {
    int required;
    std::vector<std::shared_ptr<SLC_atom>> consumed;
};

// handle difference between slc index and int literal
struct SLC_atom {
    int ival;
    std::string_view sval;
    SLC_op oval;
    SLC_atom_type type;

    static SLC_atom lambda() { return {0, "λ", {}, SLC_atom_type::LAMBDA}; }
    static SLC_atom index(int i) { return {i, {}, {}, SLC_atom_type::INDEX}; }
    static SLC_atom integer(int i) {
        return {i, {}, {}, SLC_atom_type::INTEGER};
    }
    static SLC_atom op(std::string_view s) {
        SLC_op op{};
        if (s == "+" || s == "-" || s == "*" || s == "/") {
            op.required = 2;
        }
        return {0, s, op, SLC_atom_type::OPERATOR};
    }

    std::string to_string() {
        std::string s;
        if (type == SLC_atom_type::LAMBDA) {
            s = "λ";
        } else if (type == SLC_atom_type::INDEX) {
            s = "$" + std::to_string(ival);
        } else if (type == SLC_atom_type::OPERATOR) {
            s = sval;
            for (auto arg : oval.consumed) {
                s += " :" + arg->to_string();
            }
        } else if (type == SLC_atom_type::INTEGER) {
            s = std::to_string(ival);
        }
        return s;
    }
};

// ordering
inline bool operator<(const SLC_atom &a, const SLC_atom &b) {
    if (a.type != b.type)
        return a.type < b.type;
    if (a.ival != b.ival)
        return a.ival < b.ival;
    return a.sval < b.sval;
}

inline bool operator==(const SLC_atom &a, const SLC_atom &b) {
    return a.type == b.type && a.ival == b.ival && a.sval == b.sval;
}

using SLC_element = std::variant<SLC_atom, std::shared_ptr<SLC_set>>;

struct SLC_set {
    std::set<SLC_element> elements;
    // elements are printed in a canonical order (kind, then size, then text)
    // rather than std::set order, which for nested sets is pointer order and
    // therefore varies from run to run
    std::string to_string() {
        std::vector<std::pair<size_t, std::string>> parts;
        for (const auto &elem : elements) {
            std::string s;
            if (std::holds_alternative<SLC_atom>(elem)) {
                SLC_atom atom = std::get<SLC_atom>(elem);
                s = atom.to_string();
            } else {
                s = std::get<std::shared_ptr<SLC_set>>(elem)->to_string();
            }
            parts.emplace_back(elem.index(), std::move(s));
        }
        std::sort(parts.begin(), parts.end(), [](const auto &a, const auto &b) {
            if (a.first != b.first)
                return a.first < b.first;
            if (a.second.size() != b.second.size())
                return a.second.size() < b.second.size();
            return a.second < b.second;
        });
        std::ostringstream out;
        out << "{";
        for (size_t i = 0; i < parts.size(); i++) {
            if (i)
                out << ", ";
            out << parts[i].second;
        }
        out << "}";
        return out.str();
    }
};
