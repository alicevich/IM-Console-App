// Alice Vichitthavong, Bang (Tony) Vien
// CPSC 460
// Project 3: IM Application
// IMServer.cpp
// 3/1/14

#include <sys/types.h>		// size_t, ssize_t
#include <sys/socket.h>		// socket funcs
#include <netinet/in.h>		// sockaddr_in
#include <arpa/inet.h>		// htons, inet_pton
#include <unistd.h>		// close
#include <stdio.h>		// printf, fgets 
#include <stdlib.h>		// atoi 
#include <pthread.h>
#include <netdb.h>
#include <string>
#include <iostream>
using namespace std;

struct User
{
	int sock;
	string name;
	bool isOnline;
	bool inTalkMode;
};

struct ThreadArgs
{
	int clientSock; 
	int serverSock; 
};

const int MAX_USERS = 50;
User users[MAX_USERS];

void initializeUsersList(); 
void* threadMain(void* args); 
void processLogin(int clientSock); 
void processMenuSelections(int clientSock, int serverSock);
void TalkTo(int clientSock);
void displayBuddyList(int clientSock);
void clientLogOff(int clientSock); 
bool userExists(string userName);  
void addToUsersList(string userName, int clientSock);
void notifyAllClients(string userName, string message); 
int getSock(string userName);
string getReceiverName(string protocol);
string getMessage(string protocol);
string getClientUserName(int clientSock);
int getServerSocket(unsigned short port);
void setUserAvailable(int clientSock);
void setUserUnavailable(int clientSock);
void sendToClient(int sock,string msg);
string readFromClient(int sock);
void setUserInTalkMode(int clientSock);
void undoUserInTalkMode(int clientSock);
bool isInTalkMode(string receiverName);

int main(int argc, char * argv[])
{
	if(argc < 2) {
		printf("Please start server with this format: ./IMServer <port>\n");
		return -1;
	}
	
	initializeUsersList();
	
	unsigned short servPort = atoi(argv[1]);
	int serverSock = getServerSocket(servPort);
	
	cout << "Server is currently running on Port: \n" << servPort << endl;
	  
	// Accept client connections
	while(true) { 
		//Accept connection from client
		struct sockaddr_in clientAddr;
		socklen_t addrLen = sizeof(clientAddr);
		int clientSock = accept(serverSock, (struct sockaddr *) &clientAddr, &addrLen);
		if(clientSock < 0) {
			close(serverSock);
			printf("Error: cannot set socket to listen");
			return -1; 
		}
		
		// Create and initialize argument struct
		ThreadArgs* args = new ThreadArgs;;
		args->clientSock = clientSock;
		args->serverSock = serverSock;
		
		// Create client thread 
		pthread_t threadID;
		int status = pthread_create(&threadID, NULL, threadMain, (void* ) args);
		if(status != 0) {
			printf("Error creating thread");
			return -1;
		}
	}
}

void *threadMain(void* args)
{
	// Insures thread resources are deallocated on return
	pthread_detach(pthread_self());

	// Extract socket file descriptor from argument
	struct ThreadArgs* threadArgs = (struct ThreadArgs*) args;
	int clientSock = ((ThreadArgs*) threadArgs)->clientSock;
	int serverSock = ((ThreadArgs*) threadArgs)->serverSock;

	delete threadArgs;

	// Communicate with client
	processLogin(clientSock);
	processMenuSelections(clientSock, serverSock);
  
	// Close client socket
	close(clientSock);
	pthread_exit(NULL);
}

void initializeUsersList()
{
	for(int i = 0; i < MAX_USERS; i++)
	{
		users[i].isOnline = false; 
	}
}

void processLogin(int clientSock)
{
	string userName;
	string message;
	bool firstAttempt = true;

	// Receive username from client.
	// If userName already in use, continue prompting for a new one.
	userName = readFromClient(clientSock); 
	while(userExists(userName)) 
	{
		if(!firstAttempt)
		{
			userName = readFromClient(clientSock);
			if(!userExists(userName)) break;
		}
		message = "Username taken. Please enter a new username.";
		sendToClient(clientSock, message);
		firstAttempt = false;
	} 

	sendToClient(clientSock, "Valid username");

	addToUsersList(userName, clientSock);
	
	message = userName + " has logged in.";
	notifyAllClients(userName, message);

	cout << message << endl;
}

void processMenuSelections(int clientSock, int serverSock)
{
	int nfds = clientSock < serverSock ? serverSock : clientSock; //nfds parameter is included for compatibility with Berkeley sockets.
	nfds++;
	fd_set readfds; //Optional pointer to a set of sockets to be checked for readability.
	
	bool done = false;

	while(true) 
	{
		FD_ZERO(&readfds); // Clears fd set
		FD_SET(clientSock, &readfds);
		FD_SET(serverSock, &readfds);

		int status = select(nfds, &readfds, NULL, NULL, NULL);
		if(status < 0)
		{
			printf("Error: could not select");
			exit(-1);
		} 

		if(FD_ISSET(clientSock, &readfds)) //client wants to send something
		{
			string selection = readFromClient(clientSock);
			
			switch (atoi(selection.c_str()))
			{
			case 1: 
				TalkTo(clientSock); 
				break;
			case 2:
				displayBuddyList(clientSock); 
				break;
			case 4: 
				done = true;
				clientLogOff(clientSock); 
				break;
			default:
				break;
			}
		}
		if(done) break;
	}
}

void TalkTo(int clientSock)
 {
	string clientUserName = getClientUserName(clientSock);
	
	//user in talk mode, so set user as unavailable in global buddy list
	setUserUnavailable(clientSock);
	setUserInTalkMode(clientSock);
	
	//read userName from client
	bool isFirstAttempt = true;
	bool isMessagingSelf = false;

	string receiver = readFromClient(clientSock);
	while(!userExists(receiver) || receiver == clientUserName)
	{
		if(!isFirstAttempt)
		{
			receiver = readFromClient(clientSock);
			if(receiver == clientUserName) isMessagingSelf == true;
			if(userExists(receiver) && receiver != clientUserName) break;
		}
		
		if(isMessagingSelf || receiver == clientUserName) //'receiver == clientUserName' needed only for first time entering loop
		{
			sendToClient(clientSock, "Error: messaging self");
			isMessagingSelf = false;
		}
		else 
		{
			sendToClient(clientSock, "Error: invalid username");
		}
		
		displayBuddyList(clientSock);
		isFirstAttempt = false;
	}
	
	sendToClient(clientSock, "");
	
	// Process incoming request
	string protocol = readFromClient(clientSock);
	setUserAvailable(clientSock);
	undoUserInTalkMode(clientSock);
	cout << "\n" + protocol + "\n"<< endl;
	
	string receiverName = getReceiverName(protocol);
	string senderName = clientUserName;
	string message = getMessage(protocol);

	int receiverSock = getSock(receiverName);
	message = senderName + " says: " + message;
	
	//Check that receiver is still online before forwarding message
	if(userExists(receiverName) && !isInTalkMode(receiverName))
	{
		sendToClient(receiverSock, message);
	} 
	else //Else receiver is in talking mode. 
	{
		bool done = false;
		while(!done)
		{
			if(!isInTalkMode(receiverName)) //once receiver gets out of talk mode, send message.
			{
				sendToClient(receiverSock, message);
				done = true;
			}
		}
	}
}

void displayBuddyList(int clientSock)
{
	string ListOfUsers = "Users online: \n";
	for(int i = 0; i < MAX_USERS; i++){
		if(users[i].isOnline == true && users[i].sock != clientSock)
		{
			ListOfUsers = ListOfUsers + users[i].name + "\n";
		}
	}
	sendToClient(clientSock, ListOfUsers);
}

void clientLogOff(int clientSock)
{
	for(int i = 0; i < MAX_USERS; i++){
		if(users[i].sock == clientSock)
		{
			string message = users[i].name + " has logged off.";
			cout << message << endl;
			notifyAllClients(users[i].name, message);		
			
			users[i].isOnline = false;
			users[i].name = "";
		}
	}
	close(clientSock);
}

bool userExists(string userName) 
{
	if(userName.length() < 1) return false;
	
	for(int i = 0 ; i < MAX_USERS; i++){
		if(users[i].isOnline == true && users[i].name == userName){
			return true;
		}
	}
	return false;
}

void addToUsersList(string userName, int clientSock)
{
	for(int i = 0 ; i < MAX_USERS; i++)
	{
		if(users[i].isOnline == false)
		{
			users[i].name = userName;
			users[i].sock = clientSock;
			users[i].isOnline = true;
			break;
		}
	}
}

void notifyAllClients(string userName, string message)
{
	for(int i = 0; i < MAX_USERS; i++)
	{
		if(users[i].isOnline == true && users[i].name != userName)
		{
			sendToClient(users[i].sock, message);
		}
	}  
}

int getSock(string userName)
{
	for(int i = 0; i < MAX_USERS;i++)
	{
		if(users[i].name == userName)
		{
			return users[i].sock;
		}
	}
	return 0; 
}

string getClientUserName(int clientSock)
{
	for(int i = 0; i < MAX_USERS;i++)
	{
		if(users[i].sock == clientSock)
		{
			return users[i].name;
		}
	}
}

string getReceiverName(string protocol)
{
	string name = protocol.substr(6);
	size_t end = name.find("[FROM:] ");

	return name.substr(0, end-1);
}

string getMessage(string protocol)
{
	size_t start = protocol.find("[MESSAGE:] ");
	start += 11;
	return protocol.substr(start);
}

void setUserAvailable(int clientSock) 
{
	for(int i = 0; i < MAX_USERS;i++)
	{
		if(users[i].sock == clientSock)
		{
			users[i].isOnline = true;
			break;
		}
	}
}

void setUserUnavailable(int clientSock) 
{
	for(int i = 0; i < MAX_USERS;i++)
	{
		if(users[i].sock == clientSock)
		{
			users[i].isOnline = false;
			break;
		}
	}
}

void setUserInTalkMode(int clientSock)
{
	for(int i = 0; i < MAX_USERS;i++)
	{
		if(users[i].sock == clientSock)
		{
			users[i].inTalkMode = true;
			break;
		}
	}
}

bool isInTalkMode(string receiverName)
{
	for(int i = 0; i < MAX_USERS;i++)
	{
		if(users[i].name == receiverName)
		{
			return users[i].inTalkMode;
		}
	}
}

void undoUserInTalkMode(int clientSock)
{
	for(int i = 0; i < MAX_USERS;i++)
	{
		if(users[i].sock == clientSock)
		{
			users[i].inTalkMode = false;
			break;
		}
	}
}

int getServerSocket(unsigned short servPort)
{
	// Create socket 
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sock < 0) {
		printf("Error: cannot create socket");
		return -1; 
	}

	// Set Address 
	struct sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(servPort);

	// Bind port to socket
	int status = bind(sock, (struct sockaddr *) &servAddr, sizeof(servAddr));
	if(status < 0) {
		close(sock);
		printf("Error: cannot bind socket to port");
		return -1; 
	}

	// Set socket to listen
	status = listen(sock, MAX_USERS);
	if(status < 0) {
		close(sock);
		printf("Error: cannot set socket to listen");
		return -1; 
	}
  
	return sock;
}

void sendToClient(int clientSocket,string msg)
{
	int messageLength = msg.length();
	char *message = message = new char[messageLength+1];
	strcpy(message, msg.c_str());
	// Send length of name_str
	int bytesSent = send(clientSocket, &messageLength, sizeof(int), 0);
	if(bytesSent != sizeof(int))
	{
          cerr << "wrong amount of bytes sent" << endl;
          exit(-1);	
	}
	// Send name to server
	int bytesSent1 = send(clientSocket, (void *) message, messageLength, 0);
	if(bytesSent1 != messageLength)
	{
	  cerr << "wrong amount of bytes sent" << endl;
         exit(-1);
	} 

	delete [] message;
}

string readFromClient(int clientSocket)
{
	string data;
	int messageLength;
	char bp;
	int bytesRecv = recv(clientSocket, &messageLength, sizeof(int), 0);
	while(messageLength > 0){
	bytesRecv = recv(clientSocket, (void *)&bp, 1, 0);
	if(bytesRecv < 0)
         {
           cerr << "recv failure" << endl;
	    exit(-1);
         }
       messageLength = messageLength - bytesRecv;
       data = data + bp;
	}
	return data;
}
