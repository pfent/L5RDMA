all:
	g++ -std=c++11 main.cpp Network.cpp -o bin/main -libverbs -lpthread

clean:
	rm bin/main
