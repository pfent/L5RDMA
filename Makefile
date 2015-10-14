all:
	g++ -std=c++14 -Wall -Wextra main.cpp Network.cpp WorkRequest.cpp MemoryRegion.cpp -o bin/main -libverbs -lpthread -g3 -O0

clean:
	rm bin/main
