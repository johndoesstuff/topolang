// This program is part of the larger project Topolang which aims to map 2d
// topologies to program structures. This layer acts as an informal proof that
// Set Lambda Calculus (and by extension Topolang) is turing complete. This is
// achieved by mapping the set of all Unsigned Lambda Calculus expressions to
// corresponding sets in Set Lambda Calculus.
//
// For reference the reduction rules of Set Lambda Calculus are as follows:
//
// E-Parallel : if t_1 -> t_1' and t_2 -> t_2'
//              then {t_1, t_2} -> {t_1', t_2'}
//
// E-Lambda   : {λ, v} -> λv
//
// E-Promote  : {⊕, t^n} -> t^(n+1)
// E-Demote   : {⊖, t^n} -> t^(n-1)
//
// E-Consume  : if n > ↑m
//              then {λv^n, t^m} -> Λv^n
//              and δv := t^m
//
// E-App      : if n > ↑m
//              then {Λv^n, t^m} -> δv[v |-> t^m]
//
// E-Ambig    : if t_1 -> t_1'
//              then {t_1, t_2} -> {t_1', t_2}

#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <set>
#include <variant>
#include <sstream>

enum class ULC_token_type {
	LAMBDA,
	VARIABLE,
	DOT,
	O_PAREN,
	C_PAREN,
	EOF_TOK,
};

struct ULC_token {
	ULC_token() : type(ULC_token_type::EOF_TOK) {}
	ULC_token(ULC_token_type t) : type(t) {}
	ULC_token(ULC_token_type t, std::string_view s) : type(t), text(s) {}

	ULC_token_type type;
	std::string_view text;
};

struct ULC_lexer {
	std::string_view text_;
	int pos_;
	ULC_lexer(std::string_view text) : text_(text), pos_(0) {}
	
	ULC_token next_token() {
		// EOF and whitespace handling
		if (pos_ >= text_.length()) return ULC_token(ULC_token_type::EOF_TOK);
		char ch = text_[pos_];
		while (std::isspace(ch)) {
			pos_++;
			if (pos_ >= text_.length()) return ULC_token(ULC_token_type::EOF_TOK);
			ch = text_[pos_];
		}

		int original_pos = pos_;
		if (ch == '.') {
			pos_++;
			return ULC_token(ULC_token_type::DOT, ".");
		} else if (ch == '\\') {
			pos_++;
			return ULC_token(ULC_token_type::LAMBDA, "\\");
		} else if (ch == '(') {
			pos_++;
			return ULC_token(ULC_token_type::O_PAREN, "(");
		} else if (ch == ')') {
			pos_++;
			return ULC_token(ULC_token_type::C_PAREN, ")");
		} else if (std::isalnum(ch)) {
			while (std::isalnum(ch)) {
				pos_++;
				if (pos_ >= text_.length()) break;
				ch = text_[pos_];
			}
			return ULC_token(ULC_token_type::VARIABLE, text_.substr(original_pos, pos_ - original_pos));
		} else {
			throw std::runtime_error("Invalid token");
		}
	}
};

enum class ULC_AST_type {
	APPLICATION,
	DEFINITION,
	ATOMIC,
	GROUP,
};

struct ULC_AST_node {
	ULC_AST_type type;
	std::unique_ptr<struct ULC_AST_node> right;
	std::unique_ptr<struct ULC_AST_node> left;
	ULC_token value;

	ULC_AST_node(ULC_token v) : value(v), type(ULC_AST_type::ATOMIC) {}
	ULC_AST_node() {}
};

/*
 * Parse Grammar:
 *
 * P := A | P (A | B)
 * B := 'λ' V '.' E
 * E := A | B | P
 * A := '(' E ')' | V
 *
 * Important to note this actual structure is modified in implementation to get
 * around right associativity constraints.
 */

struct ULC_parser {
	ULC_lexer lexer_;
	ULC_parser(ULC_lexer lexer) : lexer_(lexer), token_id_(0) {
		tokens_.push_back(lexer_.next_token());
	}
	std::vector<ULC_token> tokens_;
	int token_id_;

	ULC_token peek() {
		return tokens_[token_id_];
	}

	// grab next token from lexer if we haven't already
	ULC_token next_token() {
		token_id_++;
		if (token_id_ >= tokens_.size()) {
			tokens_.push_back(lexer_.next_token());
		}
		return tokens_[token_id_];
	}

	// true if token of type can be consumed
	bool consume_type(ULC_token_type type) {
		if (peek().type == type) {
			next_token();
			return true;
		}
		return false;
	}

	// ptr to ast node of consumed variable
	std::unique_ptr<ULC_AST_node> consume_variable() {
		auto node = std::make_unique<ULC_AST_node>();
		if (peek().type == ULC_token_type::VARIABLE) {
			node->type = ULC_AST_type::ATOMIC;
			node->value = peek();
			next_token();
			return node;
		}
		return nullptr;
	}

	// wrapper for parse
	std::unique_ptr<ULC_AST_node> parse() {
		return parse_expression();
	}

	// parses a function definition
	std::unique_ptr<ULC_AST_node> parse_abstraction() {
		int original_id = token_id_;
		auto node = std::make_unique<ULC_AST_node>();

		if (consume_type(ULC_token_type::LAMBDA)) {
			node->type = ULC_AST_type::DEFINITION;
			auto var = consume_variable();
			if (var != nullptr) {
				node->right = std::move(var);
				if (consume_type(ULC_token_type::DOT)) {
					auto expr = parse_expression();
					if (expr != nullptr) {
						node->left = std::move(expr);
						return node;
					}
				}
			}
		}

		token_id_ = original_id;
		return nullptr;
	}

	// parses an expression by trying to parse and abstraction then trying to
	// parse applications
	std::unique_ptr<ULC_AST_node> parse_expression() {
		// try to parse an abstraction
		if (peek().type == ULC_token_type::LAMBDA) {
			auto abs = parse_abstraction();
			return abs;
		}

		auto head = parse_atomic();
		if (!head) {
			return nullptr;
		}

		while (true) {
			// check for expression start
			if (peek().type == ULC_token_type::VARIABLE ||
					peek().type == ULC_token_type::O_PAREN) {
				auto next_arg = parse_atomic();
				if (!next_arg) break;
				auto app = std::make_unique<ULC_AST_node>();
				app->type = ULC_AST_type::APPLICATION;
				// rearrange trees for right associativity
				app->left = std::move(head);
				app->right = std::move(next_arg);
				head = std::move(app);
			} else {
				break;
			}
		}
		return head;
	}

	// parse an atomic as either (e) or v
	std::unique_ptr<ULC_AST_node> parse_atomic() {
		// try consuming a parenthesized expression
		if (consume_type(ULC_token_type::O_PAREN)) {
			auto node = std::make_unique<ULC_AST_node>();
			node->type = ULC_AST_type::GROUP;
			node->right = std::move(parse_expression());
			if (!consume_type(ULC_token_type::C_PAREN)) {
				throw std::runtime_error("Unbalanced parens!");
			}
			return node;
		}

		// otherwise consume a variable
		auto var = consume_variable();
		return var;
	}
};

// define recursive SLC set
struct SLC_set;
using SLC_element = std::variant<std::string_view, std::shared_ptr<SLC_set>>;

struct SLC_set {
	std::set<SLC_element> elements;
	std::string to_string() {
		std::ostringstream out;
		out << "{";
		for (auto it = elements.begin(); it != elements.end(); it++) {
			auto elem = *it;
			if (it != elements.begin()) out << ", ";
			if (std::holds_alternative<std::string_view>(elem)) {
				out << std::get<std::string_view>(elem);
			} else {
				out << std::get<std::shared_ptr<SLC_set>>(elem).get()->to_string();
			}
		}
		out << "}";
		return out.str();
	}
};

// driver for converting ULC to SLC sets
struct ULC_converter {
	std::unique_ptr<ULC_AST_node> root_;

	ULC_converter(std::string_view text) {
		ULC_lexer lexer(text);
		ULC_parser parser(lexer);
		root_ = std::move(parser.parse());
	}
	ULC_converter(std::unique_ptr<ULC_AST_node> root) {
		root_ = std::move(root);
	}

	SLC_set convert() {
		return convert_subset(root_.get());
	}

	SLC_set convert_subset(const ULC_AST_node* node) {
		SLC_set ret_set;
		if (!node) return ret_set;
		switch (node->type) {
			case ULC_AST_type::DEFINITION: {
				SLC_set lambda_set;
				lambda_set.elements.insert("λ");
				lambda_set.elements.insert(node->right->value.text);
				ret_set.elements.insert(std::make_shared<SLC_set>(std::move(lambda_set)));
				if (node->left.get()->type == ULC_AST_type::ATOMIC) {
					ret_set.elements.insert(node->left.get()->value.text);
				} else {
					auto sub_set = convert_subset(node->left.get());
					ret_set.elements.insert(std::make_shared<SLC_set>(std::move(sub_set)));
				} }
				return ret_set;
				break;
			case ULC_AST_type::APPLICATION: {
				SLC_set promote_set;
				promote_set.elements.insert("⊕");
				if (node->left.get()->type == ULC_AST_type::ATOMIC) {
					promote_set.elements.insert(node->left.get()->value.text);
				} else {
					auto sub_set = convert_subset(node->left.get());
					promote_set.elements.insert(std::make_shared<SLC_set>(std::move(sub_set)));
				}
				ret_set.elements.insert(std::make_shared<SLC_set>(std::move(promote_set)));
				if (node->right.get()->type == ULC_AST_type::ATOMIC) {
					ret_set.elements.insert(node->right.get()->value.text);
				} else {
					auto sub_set = convert_subset(node->right.get());
					ret_set.elements.insert(std::make_shared<SLC_set>(std::move(sub_set)));
				} }
				return ret_set;
				break;
			case ULC_AST_type::GROUP:
				return convert_subset(node->right.get());
		}
		return ret_set;
	}
};

int main() {
	ULC_lexer lexer("(\\f.(\\x.f (x x)) (\\x.f (x x))) g");
	//ULC_lexer lexer("(x)");
	//ULC_parser parser(lexer);
	//auto root = parser.parse();
	//ulc2slc(root.get());
	//std::cout << std::endl;

	ULC_converter fix_converter("(\\f.(\\x.f (x x)) (\\x.f (x x))) g");
	std::cout << fix_converter.convert().to_string() << std::endl;

	ULC_converter cond_converter("\\y.\\x.y");
	std::cout << cond_converter.convert().to_string() << std::endl;
}
