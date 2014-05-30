all: 
	g++ -o IMServer IMServer.cpp -lpthread
	g++ -o IMClient IMClient.cpp