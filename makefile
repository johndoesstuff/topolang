target:
	clang-format --style=file ulc2toposet.cpp -i
	clang-format --style=file toposet_reducer.cpp -i
	clang-format --style=file slc_set.hpp -i
	clang-format --style=file huffman_encodings.hpp -i
	mkdir -p build
	emcc -std=c++17 -Wall -lembind -o build/ulc2toposet.js ulc2toposet.cpp
	emcc -std=c++17 -Wall -lembind -sMODULARIZE=1 -sEXPORT_NAME=ReducerModule -o build/toposet_reducer.js toposet_reducer.cpp
