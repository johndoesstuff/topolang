target:
	clang-format --style=file ulc2toposet.cpp -i
	emcc -std=c++17 -Wall -lembind -o build/ulc2toposet.js ulc2toposet.cpp
