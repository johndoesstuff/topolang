target:
	clang-format --style=file src/ulc2toposet.cpp -i
	clang-format --style=file src/toposet_reducer.cpp -i
	clang-format --style=file src/slc_set.hpp -i
	clang-format --style=file src/huffman_encodings.hpp -i
	mkdir -p build
	em++ -std=c++17 -Wall -lembind -o web/build/ulc2toposet.js src/ulc2toposet.cpp
	em++ -std=c++17 -Wall -lembind -sMODULARIZE=1 -sEXPORT_NAME=ReducerModule -o web/build/toposet_reducer.js src/toposet_reducer.cpp
