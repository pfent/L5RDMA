all:
	g++ -std=c++11 -Wall -Wextra main.cpp Network.cpp -o bin/main -libverbs -lpthread

clean:
	rm bin/main
