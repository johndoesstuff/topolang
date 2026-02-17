#=
The topology of topolang can be broken into unordered Trees, computation must
then be extracted from these unordered trees. This script assumes symbols
have been inferred from a D-ary Huffman encoding and thus the problem of
computation has been reduced to arbitrarily nested unordered sets

The reduction of these sets is distinct from lambda calculus but does borrow
ideas heavily. The following are set definitions for evaluation:

F = set of all functions
V = set of all values

F_(n) = set of all functions that take n arguments
F_(c) = set of all commutative functions
C = set of all characters
N = set of all numbers
S = set of all strings

F_(n) ⊆ F
F_(c) ⊆ F
C, N, S ⊆ V

Valid Reductions:

{ f_(1), v } → v : f(v)
{ f_(n), v } → f_(n-1) : f_(n)(...v)
{ f_(c), v_(1), ...v_(n) } → v : f_(c)(v_(1), ...v_(n))

Builtin functions:
* = Commutative
+ = Variadic Arguments (Associative)

+*f_(+) : addition
+*f_(*) : multiplication
+*f_(&) : logical and
+*f_(|) : logical or
f_(-)   : subtraction
f_(/)   : division
f_(%)   : modulus

Value binding:

f_(->)(v_(1), v_(2)):
Bind value of v_(2) to v_(1)

f_(<-)(v_(1)):
Return binded value of v_(1)

There must never be more than one f_(->) for any value binded to
For any f_(<-) the value binded to's f_(->) must be evaluated first
=#

abstract type TopoNode end

struct FunctionNode <: TopoNode
	func::Function
	args::Int
	isCom::Bool
end

struct ValueNode <: TopoNode
	val::Any
end

FunctionNode(func, args) = FunctionNode(func, args, false)

# Standard Functions
F_add = FunctionNode((a, b) -> a + b, 2, true)
F_sub = FunctionNode((a, b) -> b - a, 2)
F_mul = FunctionNode((a, b) -> a * b, 2, true)
F_div = FunctionNode((a, b) -> b / a, 2)

# Program
Program = Set([F_add, ValueNode(3), ValueNode(2)])
Program_Sub = 
Set([
	Set([F_sub, ValueNode(5)]),
	ValueNode(3)
])

# Eval
function eval_program(program)
	program_array = collect(program)
	# Reduction is only possible if nodes aren't sets
	resolved_elements = Set()
	for item in program_array
		if item isa Set
			push!(resolved_elements, eval_program(item))
		else
			push!(resolved_elements, item)
		end
	end

	functions = [x for x in resolved_elements if x isa FunctionNode]
	values = [x for x in resolved_elements if x isa ValueNode]

	if length(functions) != 1
		throw("Sets must have one and only one function node")
	end

	if length(program_array) == 2
		func = functions[1]
		val = values[1]
		if func.args == 1
			# 1. { f_(1), v } → v : f(v)
			return ValueNode(func.func(val.val))
		else
			# 2. { f_(n), v } → f_(n-1) : f_(n)(...v)
			return FunctionNode((args...) -> func.func(args..., val.val), func.args - 1)
		end
	else
		func = functions[1]
		if !func.isCom
			throw("Functions with multiple values must be commutative")
		end
		# 3. { f_(c), v_(1), ...v_(n) } → v : f_(c)(v_(1), ...v_(n))
		return ValueNode(reduce(func.func, map(x -> x.val, values)))
	end
end
