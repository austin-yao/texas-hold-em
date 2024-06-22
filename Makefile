TARGETS = main client

all: $(TARGETS)

%.o: %.cpp
	g++ -std=c++17 $^ -c -o $@

main: main.o helper.o models.o
	g++ -g -std=c++17 $^ -o $@

client: client.o helper.o
	g++ -std=c++17 $^ -o $@