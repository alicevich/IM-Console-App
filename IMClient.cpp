// Bang (Tony) Vien, Alice Vichitthavong
// CPSC 460
// Project 3: IM Application
// IMClient.cpp
// 3/1/14

// select funtion explanation
// select() return the number of file descriptors contained in the three returned descriptor sets
// first argument is largest fd+1 just conforms to berkeley socket requirements
// second is set of file descriptors being watched to see if something is available for reading
// third is the set of file descriptors being watched to see if socket/fd is available for writing
// fourth is the set of file descriptors being watched for errors
// last is a timer to see how long it should wait with no response before it should stop blocking

#include <cstring> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <iostream>
#include <string>
#include <errno.h>
#include <netdb.h>
#include <sstream>
using namespace std;

// global variables
const string menu = "\n1)TalkTo\n2)Buddylist\n3)Menu\n4)SignOut\n";

// Function Prototypes
string welcome(int socketNum);
void talkTo(int socketNum,string &name);
void displayBuddyList(int socketNum);
int getServerSocket(char* host,unsigned short port);
void send(int sock,string msg);
string recv(int sock);
void trim(string& s);


int main(int argc, char *argv[])
{
	if(argc < 3){
	cerr << "Please start client with this format: ./IMClient <hostname> <portnumber>\n";
	exit(-1);
	}
	char *hostname = argv[1];
	unsigned short serverPort = atoi(argv[2]);
	int socketNum = getServerSocket(hostname,serverPort);
	string name = welcome(socketNum);

	//declaring select structures
	int count = 1;
	int STDIN = 0; //0 is the default file descriptor for STDIN not a random number
	string option;
	fd_set readFD;
	fd_set origin; 
	FD_SET(STDIN, &origin); // Add stdin to origin set
	FD_SET(socketNum,&origin); //Add server socket to the origin set
	cout << menu << endl;
	cout << "Enter choice (1,2,3,4): " << endl;
	while(true)
	{
		//resets the readFD set each time the loop starts over since select statements removes FD's that are unused
		readFD = origin; 
		if(select(socketNum+1, &readFD, NULL, NULL, NULL) == -1){
			cout << "Error occured, closing program..." << endl;
			return 1;
		}
		else
		{
			if(FD_ISSET(STDIN, &readFD)){ // standard input	
				if (count == 1){
					cin.ignore();
					count++;
				}
				getline(cin, option);

				//Switch statement to deal with chosen option
				switch (atoi(option.c_str())) 
				{
					case 1:
					send(socketNum,option);
					talkTo(socketNum,name);
					break;
					case 2:
					send(socketNum,option);
					displayBuddyList(socketNum);
					break;
					case 3:
					cout<<menu<<endl;
					break;
					case 4:
					send(socketNum,option);
					cout << "Logging Off" << endl;
					close(socketNum);
					exit(0);
				}
			}
			else if(FD_ISSET(socketNum, &readFD)) // recv something from server
			{ 
				string message = "";
				message = recv(socketNum);
				cout << message << endl;
			}
			else{}
		}
	}
	
}

string welcome(int socketNum)
{
	cout << "Logging on" << endl;
       cout << "----------" << endl;
	bool nameExists = true;
	string userName;
	string response = "";
	while(nameExists) 
	{
		cout << response << endl;
		cout << "Username: ";
		cin >> userName;
		trim(userName);
		send(socketNum, userName);
		response = recv(socketNum);

		if(response != "Username taken. Please enter a new username.")
			nameExists = false;
	}	return userName;
}

void talkTo(int serverSock, string& userName)
{
	string receiver; 
	string status;

	cout << "\nTalk To: ";
	cin >> receiver;
	trim(receiver);

	send(serverSock, receiver); //send to server check if valid name
	status = recv(serverSock);

	while(status.substr(0,5) == "Error")
	{
		if(status == "Error: messaging self")
			cout << "\nYou cannot message yourself. Please enter available username." << endl;
		else 
			cout << "\nUser is offline, doesn't exists, or is currently sending a message. Please select a different user from the buddy list or try again." << endl;
		
		displayBuddyList(serverSock);

		cout << "\nTalk to: ";
		cin >> receiver;
		trim(receiver);

		send(serverSock, receiver); //send to server check if valid name
		status = recv(serverSock); //receive status
	}
	
	//generate message to send to server
	string message; 
	cout << "Message: ";
	cin.ignore();
	getline(cin, message);

	string protocol;
	protocol = "[TO:] " + receiver 
		+ "\n[FROM:] " + userName
		+ "\n[MESSAGE:] " + message;

	send(serverSock, protocol);	
}

void displayBuddyList(int serverSock)
{
	string list;
	list = recv(serverSock);
	cout << "-----" << endl;
	cout << list;
	cout << "-----" << endl;
}

int getServerSocket(char* host,unsigned short port)
{
	int i = 0;
	char *hostname = host;
	unsigned short serverPort = port;
  
	// Get address by hostname
	const char *charhost = hostname;
	struct hostent *hen = gethostbyname(charhost);
	if(!hen)
		cerr << "Hostname Error" << endl;
  
	struct in_addr **addr_list = (struct in_addr **)gethostbyname(hostname)->h_addr_list;
	char *IPAddr;
	for(i = 0; addr_list[i] != NULL; i++){
		IPAddr = inet_ntoa(*addr_list[i]);
	}
  
	int socketNum = socket(AF_INET, SOCK_STREAM, 0);
	if(socketNum == -1)
	{  	
		cerr << "Error with socket" << endl;	
		exit(-1);
	}
	unsigned long serverIP;
	int convertstatus = inet_pton(AF_INET, IPAddr, &serverIP);
	if(convertstatus <= 0)
	{
		cerr << "IP address conversion failed" << endl;
		exit(-1);
	} 
	struct sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = serverIP;
	serverAddress.sin_port = htons(serverPort);

	int status = connect(socketNum, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
	if(status == -1)
	{
		cerr << "Error with connect, make sure to enter a valid Server Address and Port Number" << endl;
		exit(-1);
	}
	return socketNum;
}
void send(int clientSocket,string msg)
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

string recv(int clientSocket)
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

void trim(string& s)
{
	size_t p = s.find_first_not_of(" \t");
	s.erase(0, p);
  
	p = s.find_last_not_of(" \t");
	if (string::npos != p)
		s.erase(p+1);
}



