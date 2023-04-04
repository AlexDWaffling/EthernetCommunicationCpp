#include <stdio.h>
#include <enet/enet.h>
#include <iostream>
#include <string>
#include <map>
#include <pthread.h>

#include "chat_screen.hpp"
using namespace std;

static ChatScreen chatScreen;
static int CLIENT_ID = -1;

// The Client's data will be managed by this class
class ClientData
{
	private:
	int m_id;
	std::string m_username;

	public:
	ClientData(int id) : m_id(id) {}

	void SetUsername(std::string username){ m_username = username; }

	int GetID(){ return m_id; }
	std::string GetUsername(){ return m_username; }
};

// A std::map of all clients that are currently connected.
std::map<int, ClientData*> client_map; // id, ClientData*

// A simple way to send packets to a peer
void SendPacket(ENetPeer* peer, const char* data)
{
	// Create the packet using enet_packet_create and the data we want to send
	// We are using the flag ENET_PACKET_FLAG_RELIABLE that acts a bit like TCP.
	// That is to say, it'll make sure the packet makes it to the destination.
	ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);

	// Finally, send the packet to the peer on channel 0!
	enet_peer_send(peer, 0, packet);
}

void ParseData(char* data)
{
	// Will store the data type (e.g. 1, 2, etc)
	int data_type;

	// Will store the id of the client that is sending the data
	int id;

	// Get first two numbers from the data (data_type and id) and but them in their respective variables
	sscanf(data,"%d|%d", &data_type, &id);

	// Switch between the different data_types
	switch(data_type)
	{
		case 1: // data is a message
			if(id != CLIENT_ID)
			{
				// Get message and Post it using the ClientData at id's username and the parsed msg.
				char msg[80];
				sscanf(data, "%*d|%*d|%[^|]", &msg);
				chatScreen.PostMessage(const_cast<char *>(client_map[id]->GetUsername().c_str()), msg);
			}
			break;
		case 2: // data is a username
			if(id != CLIENT_ID)
			{
				// Create a new ClientData with username and add it to map at id.
				char username[80];
				sscanf(data, "%*d|%*d|%[^|]", &username);

				client_map[id] = new ClientData(id);
				client_map[id]->SetUsername(username);
			}
			break;
		case 3: // data is our ID.
			CLIENT_ID = id; // Set our id to the received id.
			break;
	}
}

void* MsgLoop(ENetHost* client)
{
	while(true)
	{
		ENetEvent event;
		while(enet_host_service(client, &event, 0) > 0)
		{
			switch(event.type)
			{
				case ENET_EVENT_TYPE_RECEIVE:
                // printf ("A packet of length %u containing %s was received from %x:%u on channel %u.\n",
                //         event.packet -> dataLength,
                //         event.packet -> data,
                //         event.peer -> address.host,
                //         event.peer -> address.port,
                //         event.channelID);
					ParseData(event.packet->data); // Parse the receiving data.
					enet_packet_destroy(event.packet);
					break;
			}
		}
	}
}
// Buffer cat
char BufferDataFrame(uint8_t dataMode){
    uint32_t buffer;
    uint8_t header = 0xAAU;
    uint8_t length = 3U;
    uint8_t syncNo = 0U;
    uint8_t reserved = 0U;

    buffer = header + length + syncNo + reserved + dataMode;

    return static_cast<char>(buffer);
}
// The main function
int main(int argc, char ** argv)
{
	// User set username
	printf("Please Enter Your Virtual Terminal?\n");
	char username[80];
	scanf("%s", &username);

	// Normal enet initialization (see part 1)
	if(enet_initialize() != 0)
	{
		fprintf(stderr, "An error occurred while initializing ENet!\n");
		return EXIT_FAILURE;
	}
	atexit(enet_deinitialize);

	ENetHost* client;
	client = enet_host_create(NULL, 1, 1, 0, 0);

	if(client == NULL)
	{
		fprintf(stderr, "An error occurred while trying to create an ENet client host!\n");
		return EXIT_FAILURE;
	}

	ENetAddress address;
	ENetEvent event;
	ENetPeer* peer;

	enet_address_set_host(&address, "127.0.0.1");
	address.port = 7777;

	peer = enet_host_connect(client, &address, 1, 0);
	if(peer == NULL)
	{
		fprintf(stderr, "No available peers for initiating an ENet connection!\n");
		return EXIT_FAILURE;
	}

	if(enet_host_service(client, &event, 5000) > 0 &&
		event.type == ENET_EVENT_TYPE_CONNECT)
	{
		puts("Connection to 127.0.0.1:7777 succeeded.");
	}
	else
	{
		enet_peer_reset(peer);
		puts("Connection to 127.0.0.1:7777 failed.");
		return EXIT_SUCCESS;
	}

	// Send The Server The User's Username
	char str_data[80] = "2|";
	strcat(str_data, username);
	SendPacket(peer, str_data);

	// Init the chatScreen
	chatScreen.Init();

    // Create a thread for receiving data
    pthread_t thread;
    pthread_create(&thread, NULL, MsgLoop, client);

	while(true)
	{
		std::string msg = chatScreen.CheckBoxInput();
		chatScreen.PostMessage(username, msg.c_str());

		char message_data[80] = "1|";
		strcat(message_data, msg.c_str());
		SendPacket(peer, message_data);
	}

    // Join thread
    pthread_join(thread, NULL);

	// Disconnect peer from server
	enet_peer_disconnect(peer, 0);

	while(enet_host_service(client, &event, 3000) > 0)
	{
		switch(event.type)
		{
			case ENET_EVENT_TYPE_RECEIVE:
				enet_packet_destroy(event.packet);
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				puts("Disconnection succeeded.");
				break;
		}
	}

	return EXIT_SUCCESS;
}