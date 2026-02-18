#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <variant>

// define recursive SLC set
struct SLC_set;
using SLC_element =
    std::variant<std::string_view, std::shared_ptr<SLC_set>, int>;

struct SLC_set {
    std::set<SLC_element> elements;
    std::string to_string() {
        std::ostringstream out;
        out << "{";
        for (auto it = elements.begin(); it != elements.end(); it++) {
            auto elem = *it;
            if (it != elements.begin())
                out << ", ";
            if (std::holds_alternative<std::string_view>(elem)) {
                out << std::get<std::string_view>(elem);
            } else if (std::holds_alternative<int>(elem)) {
                out << std::get<int>(elem);
            } else {
                out << std::get<std::shared_ptr<SLC_set>>(elem)
                           .get()
                           ->to_string();
            }
        }
        out << "}";
        return out.str();
    }
};
