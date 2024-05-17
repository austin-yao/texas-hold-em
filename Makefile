TARGETS = main client

all: $(TARGETS)

%.o: %.cpp
	g++ $^ -c -o $@

main: main.o 
	g++ $^ -o $@

client: client.o
	g++ $^ -o $@