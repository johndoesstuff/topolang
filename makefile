EMFLAGS = -std=c++17 -Wall -lembind

.PHONY: all format
all: format
	mkdir -p web/build
	em++ $(EMFLAGS) -o web/build/ulc2toposet.js src/ulc2toposet.cpp
	em++ $(EMFLAGS) -sMODULARIZE=1 -sEXPORT_NAME=ReducerModule -o web/build/toposet_reducer.js src/toposet_reducer.cpp

format:
	clang-format --style=file -i src/*.cpp src/*.hpp
