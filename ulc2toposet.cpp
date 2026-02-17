// This program is part of the larger project Topolang which aims to map 2d
// topologies to program structures. This layer acts as an informal proof that
// Set Lambda Calculus (and by extension Topolang) is turing complete and as a
// tool for converting lambda calculus expressions to Set Lambda Calculus. This
// is achieved by mapping the set of all Unsigned Lambda Calculus expressions
// to corresponding sets in Set Lambda Calculus.
//
// For reference the reduction rules of Set Lambda Calculus are as follows:
//
// Reduce all leaf sets using D-ary Huffman Encoding:
//
// {{}}                   -> λ
// {{{}, {}}}             -> 1
// {{{}, {}}, {}}         -> 2
// {{{}, {}}, {}, {}}     -> 3
// {{{}, {}}, {}, {}, ..} -> n
//
// This reduces to an arbitrarily nested set of lambdas and numbers. To extract
// ordering the pattern of
//
// {{n_1}, n_2}
//
// implies an ordering of n_1 n_2. From this we can reconstruct a De Bruijn
// index lambda calculus.

#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <set>
#include <variant>
#include <sstream>
#include <algorithm>

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
using SLC_element = std::variant<std::string_view, std::shared_ptr<SLC_set>, int>;

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
			} else if (std::holds_alternative<int>(elem)) {
				out << std::get<int>(elem);
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
		std::vector<std::string_view> captured;
		return convert_subset(root_.get(), 0, captured);
	}
	
	SLC_set convert_dbj() {
		std::vector<std::string_view> captured;
		return convert_subset_dbj(root_.get(), 0, captured);
	}

	SLC_set convert_subset_dbj(const ULC_AST_node* node, int lambda_depth, std::vector<std::string_view> captured) {
		SLC_set ret_set;
		if (!node) return ret_set;
		switch (node->type) {
			case ULC_AST_type::DEFINITION: {
					ret_set.elements.insert("λ");
					captured.insert(captured.begin(), node->right.get()->value.text);
					if (node->left.get()->type == ULC_AST_type::ATOMIC) {
						// ret_set.elements.insert(node->left.get()->value.text);
						std::string_view var_name = node->left.get()->value.text;
						auto it = std::find(captured.begin(), captured.end(), var_name);
						if (it == captured.end()) throw std::runtime_error("Unknown variable");
						int position = std::distance(captured.begin(), it) + 1;
						ret_set.elements.insert(position);
					} else {
						auto sub_set = convert_subset_dbj(node->left.get(), lambda_depth + 1, captured);
						ret_set.elements.insert(std::make_shared<SLC_set>(std::move(sub_set)));
					}
					captured.erase(captured.begin());
				}
				return ret_set;
				break;
			case ULC_AST_type::APPLICATION: {
					SLC_set promote_set;
					if (node->left.get()->type == ULC_AST_type::ATOMIC) {
						std::string_view var_name = node->left.get()->value.text;
						auto it = std::find(captured.begin(), captured.end(), var_name);
						if (it == captured.end()) throw std::runtime_error("Unknown variable");
						int position = std::distance(captured.begin(), it) + 1;
						promote_set.elements.insert(position);
					} else {
						auto sub_set = convert_subset_dbj(node->left.get(), lambda_depth, captured);
						promote_set.elements.insert(std::make_shared<SLC_set>(std::move(sub_set)));
					}
					ret_set.elements.insert(std::make_shared<SLC_set>(std::move(promote_set)));
					if (node->right.get()->type == ULC_AST_type::ATOMIC) {
						std::string_view var_name = node->right.get()->value.text;
						auto it = std::find(captured.begin(), captured.end(), var_name);
						if (it == captured.end()) throw std::runtime_error("Unknown variable");
						int position = std::distance(captured.begin(), it) + 1;
						ret_set.elements.insert(position);
					} else {
						auto sub_set = convert_subset_dbj(node->right.get(), lambda_depth, captured);
						ret_set.elements.insert(std::make_shared<SLC_set>(std::move(sub_set)));
					}
				}
				return ret_set;
				break;
			case ULC_AST_type::GROUP:
				return convert_subset_dbj(node->right.get(), lambda_depth, captured);
		}
		return ret_set;
	}

	SLC_set make_number(int number) {
		// 1 = {{{},{}}}
		// 2 = {{{},{}}, {}}
		// 3 = {{{},{}}, {}, {}}
		// ...
		SLC_set num_base_r;
		SLC_set num_base_l;
		SLC_set num_base;
		num_base.elements.insert(std::make_shared<SLC_set>(std::move(num_base_r)));
		num_base.elements.insert(std::make_shared<SLC_set>(std::move(num_base_l)));
		SLC_set num_top;
		num_top.elements.insert(std::make_shared<SLC_set>(std::move(num_base)));
		for (int i = 1; i < number; i++) {
			SLC_set extra;
			num_top.elements.insert(std::make_shared<SLC_set>(std::move(extra)));
		}
		return num_top;
	}

	SLC_set convert_subset(const ULC_AST_node* node, int lambda_depth, std::vector<std::string_view> captured) {
		SLC_set ret_set;
		if (!node) return ret_set;
		switch (node->type) {
			case ULC_AST_type::DEFINITION: {
					SLC_set lambda_top;
					SLC_set lambda_base;
					lambda_top.elements.insert(std::make_shared<SLC_set>(std::move(lambda_base)));
					ret_set.elements.insert(std::make_shared<SLC_set>(std::move(lambda_top)));
					captured.insert(captured.begin(), node->right.get()->value.text);
					if (node->left.get()->type == ULC_AST_type::ATOMIC) {
						std::string_view var_name = node->left.get()->value.text;
						auto it = std::find(captured.begin(), captured.end(), var_name);
						if (it == captured.end()) throw std::runtime_error("Unknown variable");
						int position = std::distance(captured.begin(), it) + 1;
						ret_set.elements.insert(std::make_shared<SLC_set>(std::move(make_number(position))));
					} else {
						auto sub_set = convert_subset(node->left.get(), lambda_depth + 1, captured);
						ret_set.elements.insert(std::make_shared<SLC_set>(std::move(sub_set)));
					}
					captured.erase(captured.begin());
				}
				return ret_set;
				break;
			case ULC_AST_type::APPLICATION: {
					SLC_set promote_set;
					if (node->left.get()->type == ULC_AST_type::ATOMIC) {
						std::string_view var_name = node->left.get()->value.text;
						auto it = std::find(captured.begin(), captured.end(), var_name);
						if (it == captured.end()) throw std::runtime_error("Unknown variable");
						int position = std::distance(captured.begin(), it) + 1;
						promote_set.elements.insert(std::make_shared<SLC_set>(std::move(make_number(position))));
					} else {
						auto sub_set = convert_subset(node->left.get(), lambda_depth, captured);
						promote_set.elements.insert(std::make_shared<SLC_set>(std::move(sub_set)));
					}
					ret_set.elements.insert(std::make_shared<SLC_set>(std::move(promote_set)));
					if (node->right.get()->type == ULC_AST_type::ATOMIC) {
						std::string_view var_name = node->right.get()->value.text;
						auto it = std::find(captured.begin(), captured.end(), var_name);
						if (it == captured.end()) throw std::runtime_error("Unknown variable");
						int position = std::distance(captured.begin(), it) + 1;
						ret_set.elements.insert(std::make_shared<SLC_set>(std::move(make_number(position))));
					} else {
						auto sub_set = convert_subset(node->right.get(), lambda_depth, captured);
						ret_set.elements.insert(std::make_shared<SLC_set>(std::move(sub_set)));
					}
				}
				return ret_set;
				break;
			case ULC_AST_type::GROUP:
				return convert_subset(node->right.get(), lambda_depth, captured);
				break;
		}
		throw std::runtime_error("Huh??");
		return ret_set;
	}
};

void display(std::string_view str) {
	std::cout << "λ: " << str << std::endl;
	ULC_converter converter(str);
	std::cout << "De Bruijn: " << converter.convert_dbj().to_string() << std::endl;
	std::cout << "SLC: " << converter.convert().to_string() << std::endl << std::endl;
}

int main() {
	std::cout << "Identity:" << std::endl;
	display("\\x.x");
	std::cout << "K-Combinator:" << std::endl;
	display("\\x.\\y.x");
	std::cout << "S-Combinator:" << std::endl;
	display("\\x.\\y.\\z.((x z)(y z))");
	std::cout << "Fixed-Point Combinator:" << std::endl;
	display("(\\f.(\\x.f (x x)) (\\x.f (x x)))");
}
