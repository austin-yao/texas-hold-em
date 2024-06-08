TARGETS = main client

all: $(TARGETS)

%.o: %.cpp
	g++ $^ -c -o $@

main: main.o helper.o models.o
	g++ -g $^ -o $@

client: client.o helper.o
	g++ $^ -o $@