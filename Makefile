TARGETS = main client

SRCS = $(wildcard src/*.cpp)

SHARED_SRC = src/helper.cpp

MAIN_SRC = src/main.cpp src/models.cpp src/game.cpp $(SHARED_SRC)

CLIENT_SRC = src/client.cpp $(SHARED_SRC)

SHARED_OBJ = $(patsubst src/%.cpp,build/%.o,$(SHARED_SRC))
OBJ_MAIN = $(patsubst src/%.cpp,build/%.o,$(MAIN_SRC))
CLIENT_OBJ = $(patsubst src/%.cpp,build/%.o,$(CLIENT_SRC))

all: $(TARGETS)

build/%.o: src/%.cpp
	mkdir -p build
	g++ -g -std=c++17 -c $< -o $@

main: $(OBJ_MAIN)
	g++ -g -std=c++17 $^ -o $@

client: $(CLIENT_OBJ)
	g++ -std=c++17 $^ -o $@