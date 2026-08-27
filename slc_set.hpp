#include <algorithm>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

// define recursive SLC set
struct SLC_set;
using SLC_element =
    std::variant<std::string_view, std::shared_ptr<SLC_set>, int>;

struct SLC_set {
    std::set<SLC_element> elements;
    // elements are printed in a canonical order (kind, then size, then text)
    // rather than std::set order, which for nested sets is pointer order and
    // therefore varies from run to run
    std::string to_string() {
        std::vector<std::pair<size_t, std::string>> parts;
        for (const auto &elem : elements) {
            std::string s;
            if (std::holds_alternative<std::string_view>(elem)) {
                s = std::string(std::get<std::string_view>(elem));
            } else if (std::holds_alternative<int>(elem)) {
                s = std::to_string(std::get<int>(elem));
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
