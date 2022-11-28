#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/random.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <string>
#include <map>
#include <iostream>
#include <time.h>

using namespace std;

#include "duckchat.h"

#define MAX_CONNECTIONS 10
#define HOSTNAME_MAX 100
#define MAX_MESSAGE_LEN 65536

typedef map<string,struct sockaddr_in> channel_type; //<username, sockaddr_in of user>

int s; //socket for listening
struct sockaddr_in server;

map<string,struct sockaddr_in> usernames; //<username, sockaddr_in of user>
map<string,int> active_usernames; //0-inactive , 1-active
map<string,string> rev_usernames; //<ip+port in string, username>
map<string,channel_type> channels;


// channel topology storage
typedef map<string,struct sockaddr_in> routing_table; // <ip+port in string, sockaddr_in>

routing_table neighbors; //<server_num, sockaddr_in of server>

map<string, routing_table> channel_tables; // <channel_name, channel_neighbors> hold the channel topologies

map<unsigned long,int> message_ids;  // 0-unseen, 1-seen

void handle_socket_input();
void handle_login_message(void *data, struct sockaddr_in sock);
void handle_logout_message(struct sockaddr_in sock);
void handle_join_message(void *data, struct sockaddr_in sock);
void handle_leave_message(void *data, struct sockaddr_in sock);
void handle_say_message(void *data, struct sockaddr_in sock);
void handle_list_message(struct sockaddr_in sock);
void handle_who_message(void *data, struct sockaddr_in sock);
void handle_keep_alive_message(struct sockaddr_in sock);
void send_error_message(struct sockaddr_in sock, string error_msg);

void send_S2S_join(struct sockaddr_in sock, string channel);
void handle_S2S_join(void *data, struct sockaddr_in sock); 

void send_S2S_leave(struct sockaddr_in sock, string channel); 
void handle_S2S_leave(void *data, struct sockaddr_in sock);

void send_S2S_say(struct sockaddr_in sock, unsigned long unique_id, string username, string channel, string text);
void handle_S2S_say(void *data, struct sockaddr_in sock);

string host_ip;

int s2s_mode = 0;
int num_neighbors = 0;
int is_leaf = 0;

int main(int argc, char *argv[])
{
	
	if ((argc < 3) || !(argc % 2))
	{
		printf("Usage: ./server domain_name port_num [domain_name port_num...]\n");
		exit(1);
	}

	char hostname[HOSTNAME_MAX];
	int port;
	
	strcpy(hostname, argv[1]);
	port = atoi(argv[2]);

	// Keep track of the neighboring servers or topology
    if (argc > 3) {
        s2s_mode = 1;
        num_neighbors = (argc-3)/2;
		if (num_neighbors == 1) is_leaf = 1;
        
        struct hostent *s_he;
        struct sockaddr_in neighbor_addr;
        int index = 0;

        for (int i = 0; index < num_neighbors; i+=2) {
            neighbor_addr.sin_family = AF_INET;
            neighbor_addr.sin_port = htons(atoi(argv[i+4]));
            if ((s_he = gethostbyname(argv[i+3])) == NULL) {
                puts("error resolving hostname...");
                exit(1);
            }
            memcpy(&neighbor_addr.sin_addr, s_he->h_addr_list[0], s_he->h_length);
			string ip_port = ip_port + inet_ntoa(neighbor_addr.sin_addr);
    		ip_port = ip_port + ':';
            ip_port = ip_port + to_string(ntohs(neighbor_addr.sin_port));
            neighbors[ip_port] = neighbor_addr;
            ip_port = "";
            index++;
        }
    }

    s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0)
	{
		perror ("socket() failed\n");
		exit(1);
	}

	struct hostent     *he;

	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if ((he = gethostbyname(hostname)) == NULL) {
		puts("error resolving hostname...");
		exit(1);
	}
	memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);

	host_ip = host_ip + inet_ntoa(server.sin_addr);
    host_ip = host_ip + ':' + argv[2];

    int err;

	err = bind(s, (struct sockaddr*)&server, sizeof server);

	if (err < 0)
	{
		perror("bind failed\n");
	}

	//testing maps end

	//create default channel Common
	string default_channel = "Common";
	map<string,struct sockaddr_in> default_channel_users;
	channels[default_channel] = default_channel_users;
    if (s2s_mode) channel_tables[default_channel] = neighbors;
	
    while(1) //server runs forever
	{

		//use a file descriptor with a timer to handle timeouts
		int rc;
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(s, &fds);
		// TODO: ADD TIMEOUT TIMER
		rc = select(s+1, &fds, NULL, NULL, NULL);
		
		if (rc < 0)
		{
			printf("error in select\n");
            getchar();
		}
		else
		{
			if (FD_ISSET(s,&fds))
			{
               
				//reading from socket
				handle_socket_input();
			}
		}	
	}
	return 0;
}



void handle_socket_input()
{

	struct sockaddr_in recv_client;
	ssize_t bytes;
	void *data;
	size_t len;
	socklen_t fromlen;
	fromlen = sizeof(recv_client);
	char recv_text[MAX_MESSAGE_LEN];
	data = &recv_text;
	len = sizeof recv_text;

	bytes = recvfrom(s, data, len, 0, (struct sockaddr*)&recv_client, &fromlen);

	if (bytes < 0)
	{
		perror ("recvfrom failed\n");
	}
	else
	{
		struct request* request_msg;
		request_msg = (struct request*)data;

		request_t message_type = request_msg->req_type;

		if (message_type == REQ_LOGIN)
		{
			handle_login_message(data, recv_client); //some methods would need recv_client
		}
		else if (message_type == REQ_LOGOUT)
		{
			handle_logout_message(recv_client);
		}
		else if (message_type == REQ_JOIN)
		{
			handle_join_message(data, recv_client);
		}
		else if (message_type == REQ_LEAVE)
		{
			handle_leave_message(data, recv_client);
		}
		else if (message_type == REQ_SAY)
		{
			handle_say_message(data, recv_client);
		}
		else if (message_type == REQ_LIST)
		{
			handle_list_message(recv_client);
		}
		else if (message_type == REQ_WHO)
		{
			handle_who_message(data, recv_client);
		}
		else if (message_type == S2S_JOIN)
		{
			handle_S2S_join(data, recv_client);
		}
        else if (message_type == S2S_LEAVE)
        {
            handle_S2S_leave(data, recv_client);
        }
        else if (message_type == S2S_SAY)
        {
            handle_S2S_say(data, recv_client);
        }
		else
		{
			//send error message to client
			send_error_message(recv_client, "*Unknown command");
		}
	}
}



void handle_login_message(void *data, struct sockaddr_in sock)
{
	struct request_login* msg;
	msg = (struct request_login*)data;

	string username = msg->req_username;
	usernames[username]	= sock;
	active_usernames[username] = 1;

	string ip = inet_ntoa(sock.sin_addr);
	int port = sock.sin_port;
 	
	char port_str[6];
 	sprintf(port_str, "%d", port);

	string key = ip + "." +port_str;
	
	rev_usernames[key] = username;

    sprintf(port_str, "%d", ntohs(port));

    string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "recv Request Login";
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;
}

void handle_logout_message(struct sockaddr_in sock)
{

	//construct the key using sockaddr_in
	string ip = inet_ntoa(sock.sin_addr);
	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);

	string key = ip + "." +port_str;

    sprintf(port_str, "%d", ntohs(port));
	
    //check whether key is in rev_usernames
	map <string,string> :: iterator iter;
    
	string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "recv Request Logout";
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;

	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//send an error message saying not logged in
		send_error_message(sock, "Not logged in");
	}
	else
	{
		string username = rev_usernames[key];
		rev_usernames.erase(iter);

		//remove from usernames
		map<string,struct sockaddr_in>::iterator user_iter;
		user_iter = usernames.find(username);
		usernames.erase(user_iter);

		//remove from all the channels if found
		map<string,channel_type>::iterator channel_iter;
		for(channel_iter = channels.begin(); channel_iter != channels.end(); channel_iter++)
		{
			map<string,struct sockaddr_in>::iterator within_channel_iterator;
			within_channel_iterator = channel_iter->second.find(username);
			if (within_channel_iterator != channel_iter->second.end())
			{
				channel_iter->second.erase(within_channel_iterator);
			}

		}

		//remove entry from active usernames also
		map<string,int>::iterator active_user_iter;
		active_user_iter = active_usernames.find(username);
		active_usernames.erase(active_user_iter);
	}
	//if so delete it and delete username from usernames
	//if not send an error message - later
}

void handle_join_message(void *data, struct sockaddr_in sock)
{
	//check whether the user is in usernames
	//if yes check whether channel is in channels
	//if channel is there add user to the channel
	//if channel is not there add channel and add user to the channel
	
	//get message fields
	struct request_join* msg;
	msg = (struct request_join*)data;

	string channel = msg->req_channel;

	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + "." +port_str;

    sprintf(port_str, "%d", ntohs(port));
	
	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;

    string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "recv Request Join " + channel;
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;
	
    iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in");
	}
	else
	{
		string username = rev_usernames[key];

		map<string,channel_type>::iterator channel_iter;

        map<string,routing_table>::iterator server_channel_iter;

		channel_iter = channels.find(channel);

        server_channel_iter = channel_tables.find(channel);

		active_usernames[username] = 1;

		if (channel_iter == channels.end())
		{
			//channel not found
			map<string,struct sockaddr_in> new_channel_users;
			new_channel_users[username] = sock;
			channels[channel] = new_channel_users;

            if (s2s_mode) channel_tables[channel] = neighbors;
		}
		else
		{
			//channel already exits
			channels[channel][username] = sock;
		}
        // send s2s_join to all neighbors
        if (s2s_mode) 
        {
            if (server_channel_iter == channel_tables.end())
            {    
                channel_tables[channel] = neighbors;
                send_S2S_join(sock, channel); 
            }
        }
	}
}



void handle_leave_message(void *data, struct sockaddr_in sock)
{
	//check whether the user is in usernames
	//if yes check whether channel is in channels
	//check whether the user is in the channel
	//if yes, remove user from channel
	//if not send an error message to the user


	//get message fields
	struct request_leave* msg;
	msg = (struct request_leave*)data;

	string channel = msg->req_channel;

	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + "." +port_str;

    sprintf(port_str, "%d", ntohs(port));
	

	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;

    string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "recv Request Leave " + channel;
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;

	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in");
	}
	else
	{
		string username = rev_usernames[key];

		map<string,channel_type>::iterator channel_iter;

		channel_iter = channels.find(channel);

		active_usernames[username] = 1;

		if (channel_iter == channels.end())
		{
			//channel not found
			send_error_message(sock, "No channel by the name " + channel);
		}
		else
		{
			//channel already exits
			map<string,struct sockaddr_in>::iterator channel_user_iter;
			channel_user_iter = channels[channel].find(username);

			if (channel_user_iter == channels[channel].end())
			{
				//user not in channel
				send_error_message(sock, "You are not in channel " + channel);
			}
			else
			{
				channels[channel].erase(channel_user_iter);

				//delete channel if no more users
				if (channels[channel].empty() && (channel != "Common"))
				{
					channels.erase(channel_iter);
				}

			}
		}
	}
}



void handle_say_message(void *data, struct sockaddr_in sock)
{
	//check whether the user is in usernames
	//if yes check whether channel is in channels
	//check whether the user is in the channel
	//if yes send the message to all the members of the channel
	//if not send an error message to the user

	//get message fields
	struct request_say* msg;
	msg = (struct request_say*)data;

	string channel = msg->req_channel;
	string text = msg->req_text;

	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + "." +port_str;

    sprintf(port_str, "%d", ntohs(port));
	
	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;

    string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "recv Request Say " + channel + ' ' + '\"' + text + '\"';
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;
	
    iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in ");
	}
	else
	{
		string username = rev_usernames[key];

		map<string,channel_type>::iterator channel_iter;

		channel_iter = channels.find(channel);

		active_usernames[username] = 1;

		if (channel_iter == channels.end())
		{
			//channel not found
			send_error_message(sock, "No channel by the name " + channel);
		}
		else
		{
			//channel already exits
			map<string,struct sockaddr_in>::iterator channel_user_iter;
			channel_user_iter = channels[channel].find(username);

			if (channel_user_iter == channels[channel].end())
			{
				//user not in channel
				send_error_message(sock, "You are not in channel " + channel);
			}
			else
			{
				map<string,struct sockaddr_in> existing_channel_users;
				existing_channel_users = channels[channel];
				for(channel_user_iter = existing_channel_users.begin(); channel_user_iter != existing_channel_users.end(); channel_user_iter++)
				{
					ssize_t bytes;
					void *send_data;
					size_t len;

					struct text_say send_msg;
					send_msg.txt_type = TXT_SAY;

					const char* str = channel.c_str();
					strcpy(send_msg.txt_channel, str);
					str = username.c_str();
					strcpy(send_msg.txt_username, str);
					str = text.c_str();
					strcpy(send_msg.txt_text, str);
					
					send_data = &send_msg;

					len = sizeof send_msg;

					struct sockaddr_in send_sock = channel_user_iter->second;

					bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

					if (bytes < 0)
					{
						perror("Message failed\n"); //error
					}
				}
                
				if (s2s_mode)
                {
                    // send message to other servers in routing table
	                // create 64-bit unique id 
                    unsigned long unique_id;
                    getrandom(&unique_id, sizeof(unsigned long), 0);

                    message_ids[unique_id] = 1;
                    send_S2S_say(sock, unique_id, username, channel, text);
			    }
            }
		}
	}
}



void handle_list_message(struct sockaddr_in sock)
{
	//check whether the user is in usernames
	//if yes, send a list of channels
	//if not send an error message to the user

	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + "." +port_str;

    sprintf(port_str, "%d", ntohs(port));
	
	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;
    
    string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "recv Request List";
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;

	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in ");
	}
	else
	{
		string username = rev_usernames[key];
		int size = channels.size();

		active_usernames[username] = 1;

		ssize_t bytes;
		void *send_data;
		size_t len;

		struct text_list *send_msg = (struct text_list*)malloc(sizeof (struct text_list) + (size * sizeof(struct channel_info)));

		send_msg->txt_type = TXT_LIST;

		send_msg->txt_nchannels = size;

		map<string,channel_type>::iterator channel_iter;

		int pos = 0;

		for(channel_iter = channels.begin(); channel_iter != channels.end(); channel_iter++)
		{
			string current_channel = channel_iter->first;
			const char* str = current_channel.c_str();
			strcpy(((send_msg->txt_channels)+pos)->ch_channel, str);

			pos++;

		}

		send_data = send_msg;
		len = sizeof (struct text_list) + (size * sizeof(struct channel_info));

		struct sockaddr_in send_sock = sock;

		bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

		if (bytes < 0)
		{
			perror("Message failed\n"); //error
		}
	}
}



void handle_who_message(void *data, struct sockaddr_in sock)
{
	//check whether the user is in usernames
	//if yes check whether channel is in channels
	//if yes, send user list in the channel
	//if not send an error message to the user

	//get message fields
	struct request_who* msg;
	msg = (struct request_who*)data;

	string channel = msg->req_channel;

	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + "." +port_str;

    sprintf(port_str, "%d", ntohs(port));
	
	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;

    string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "recv Request Who ";
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;

	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in ");
	}
	else
	{
		string username = rev_usernames[key];

		active_usernames[username] = 1;

		map<string,channel_type>::iterator channel_iter;

		channel_iter = channels.find(channel);

		if (channel_iter == channels.end())
		{
			//channel not found
			send_error_message(sock, "No channel by the name " + channel);
		}
		else
		{
			//channel exits
			map<string,struct sockaddr_in> existing_channel_users;
			existing_channel_users = channels[channel];
			int size = existing_channel_users.size();

			ssize_t bytes;
			void *send_data;
			size_t len;

			struct text_who *send_msg = (struct text_who*)malloc(sizeof (struct text_who) + (size * sizeof(struct user_info)));

			send_msg->txt_type = TXT_WHO;

			send_msg->txt_nusernames = size;

			const char* str = channel.c_str();

			strcpy(send_msg->txt_channel, str);

			map<string,struct sockaddr_in>::iterator channel_user_iter;

			int pos = 0;

			for(channel_user_iter = existing_channel_users.begin(); channel_user_iter != existing_channel_users.end(); channel_user_iter++)
			{
				string username = channel_user_iter->first;

				str = username.c_str();

				strcpy(((send_msg->txt_users)+pos)->us_username, str);

				pos++;
			}

			send_data = send_msg;
			len = sizeof(struct text_who) + (size * sizeof(struct user_info));

			struct sockaddr_in send_sock = sock;

			bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

			if (bytes < 0)
			{
				perror("Message failed\n"); //error
			}
		}
	}
}



void send_error_message(struct sockaddr_in sock, string error_msg)
{
	ssize_t bytes;
	void *send_data;
	size_t len;

	struct text_error send_msg;
	send_msg.txt_type = TXT_ERROR;

	const char* str = error_msg.c_str();
	strcpy(send_msg.txt_error, str);

	send_data = &send_msg;

	len = sizeof send_msg;

	struct sockaddr_in send_sock = sock;
    
	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", ntohs(port));

	bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

	if (bytes < 0)
	{
		perror("Message failed\n"); //error
	}

    string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "send Text Error";
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;
}



void send_S2S_join(struct sockaddr_in sender, string channel) 
{
	ssize_t bytes;
	void *send_data;
    size_t len;

	struct s2s_join send_msg;
	send_msg.req_type = S2S_JOIN;
    strcpy(send_msg.req_channel, channel.c_str());

	send_data = &send_msg;

    len = sizeof send_msg;

	struct sockaddr_in send_sock;

	for (routing_table::iterator iter = channel_tables[channel].begin(); iter != channel_tables[channel].end(); iter++) 
	{
		if (!(iter->second.sin_addr.s_addr == sender.sin_addr.s_addr && 
			iter->second.sin_port == sender.sin_port)) 
		{
			send_sock = iter->second;

			string ip = inet_ntoa(send_sock.sin_addr);

			char port_str[6];
			sprintf(port_str, "%d", ntohs(send_sock.sin_port));

			bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

			if (bytes < 0)
			{
				perror("Message failed\n"); //error
			}
			else
			{
				//printf("Message sent\n");
			}
		
			string debug_ips = host_ip + ' ' + ip + ':' + port_str;
			string debug_details = "send S2S Join " + channel;
			string debug_msg = debug_ips + ' ' + debug_details;
			cout << debug_msg << endl;
		}
	}
}



void handle_S2S_join(void *data, struct sockaddr_in sock) 
{
    struct s2s_join* msg; 
    msg = (struct s2s_join*) data; 

    string channel = msg->req_channel;
 
    string ip = inet_ntoa(sock.sin_addr);

 	char port_str[6];
 	sprintf(port_str, "%d", ntohs(sock.sin_port));
    
    string ip_port = ip_port + ip + ":" + port_str;

    string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "recv S2S Join " + channel;
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;

    if (channel_tables.find(channel) == channel_tables.end())
    {
        channel_tables[channel] = neighbors;
    }
	else
	{
        channel_tables[channel][ip_port] = sock;
	}
    send_S2S_join(sock, channel);

	// TODO reset the timer for the renewed server
}



void send_S2S_leave(struct sockaddr_in sock, string channel)
{
	// create packet send to sock
	ssize_t bytes;
	void *send_data;
    size_t len;

	struct s2s_leave send_msg;
	send_msg.req_type = S2S_LEAVE;
    strcpy(send_msg.req_channel, channel.c_str());

	send_data = &send_msg;

    len = sizeof send_msg;

	string ip = inet_ntoa(sock.sin_addr);

	char port_str[6];
	sprintf(port_str, "%d", ntohs(sock.sin_port));

	bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&sock, sizeof sock);

	if (bytes < 0)
	{
        cout<<host_ip<<" failure"<<endl;
		perror("Message failed\n"); //error
	}

	// print debug send line
	string debug_ips = host_ip + ' ' + ip + ':' + port_str;
	string debug_details = "send S2S Leave " + channel;
	string debug_msg = debug_ips + ' ' + debug_details;
	cout << debug_msg << endl;
}



void handle_S2S_leave(void *data, struct sockaddr_in sock) 
{
	struct s2s_join* msg; 
    msg = (struct s2s_join*) data; 

    string channel = msg->req_channel;
 
    string ip = inet_ntoa(sock.sin_addr);

 	char port_str[6];
 	sprintf(port_str, "%d", ntohs(sock.sin_port));
    
	// print debug recv line 
    string debug_ips = host_ip + ' ' + ip + ':' + port_str;
    string debug_details = "recv S2S Leave " + channel;
    string debug_msg = debug_ips + ' ' + debug_details;
    cout << debug_msg << endl;
    
	// remove the sending sock from the routing table
	string ip_port = ip + ":"; 
	ip_port = ip_port + port_str;
	channel_tables[channel].erase(ip_port);
    
	// check if this server needs to leave as well
	map<string, struct sockaddr_in> existing_channel_users;
    existing_channel_users = channels[channel];
	// check if any users are connected to this channel.
    if (existing_channel_users.empty())
    {
        // check if any servers rely on this server
		if  (is_leaf || (channel_tables[channel]).size() == 1) 
		{ 
			// send a leave if not to the above
			send_S2S_leave(channel_tables[channel].begin()->second, channel);
			// erase the entry at channel
			channel_tables.erase(channel);
		}
	}

}



void send_S2S_say(struct sockaddr_in sender, unsigned long unique_id, string username, string channel, string text) 
{	
	// create packet 
	ssize_t bytes;
	void *send_data;
    size_t len;

	struct s2s_say send_msg;

	send_msg.req_type = S2S_SAY;
    send_msg.unique_id = unique_id;
	strcpy(send_msg.req_username, username.c_str());
    strcpy(send_msg.req_channel, channel.c_str());
	strcpy(send_msg.req_text, text.c_str());

	send_data = &send_msg;

    len = sizeof send_msg;

	struct sockaddr_in send_sock;

	for (routing_table::iterator iter = channel_tables[channel].begin(); iter != channel_tables[channel].end(); iter++) 
	{
		if (!(iter->second.sin_addr.s_addr == sender.sin_addr.s_addr && 
			iter->second.sin_port == sender.sin_port)) 
		{
			send_sock = iter->second;

			string ip = inet_ntoa(send_sock.sin_addr);

			char port_str[6];
			sprintf(port_str, "%d", ntohs(send_sock.sin_port));

			bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

			if (bytes < 0)
			{
				perror("Message failed\n"); //error
			}
		
			// print debug send line
			string debug_ips = host_ip + ' ' + ip + ':' + port_str;
			string debug_details = "send S2S Say " + username + " "  + channel + " \"" + text + "\"";
			string debug_msg = debug_ips + ' ' + debug_details;
			cout << debug_msg << endl;
		}
	}		
}



void handle_S2S_say(void *data, struct sockaddr_in sock)
{
	// loop detection ignore if message id matches one already received 
	// if not
		// relay the say message to other servers in channel topology 
		// relay to all users on this server

	struct s2s_say* msg; 
    msg = (struct s2s_say*) data; 
    
    string channel = msg->req_channel;
	
    if (!message_ids[msg->unique_id])  
	{
		message_ids[msg->unique_id] = 1;

		string username = msg->req_username;
		string text = msg->req_text;

		string ip = inet_ntoa(sock.sin_addr);

		char port_str[6];
		sprintf(port_str, "%d", ntohs(sock.sin_port));
		
		// print debug recv line
		string debug_ips = host_ip + ' ' + ip + ':' + port_str;
		string debug_details = "recv S2S Say " + username + " " + channel + " \"" + text + "\"";
		string debug_msg = debug_ips + ' ' + debug_details;
		cout << debug_msg << endl;
		
		// forward the message to routing table
		send_S2S_say(sock, msg->unique_id, username, channel, text);

		// send message to users subscribed to channel on this server
        map<string, struct sockaddr_in>::iterator channel_user_iter;
        
        map<string, struct sockaddr_in> existing_channel_users;
        existing_channel_users = channels[channel];
        if (!existing_channel_users.empty())
        {
            for (channel_user_iter = existing_channel_users.begin(); channel_user_iter != existing_channel_users.end(); channel_user_iter++) 
            {
                ssize_t bytes;
                void *send_data;
                size_t len;

                struct text_say send_msg;
                send_msg.txt_type = TXT_SAY;

                const char* str = channel.c_str();
                strcpy(send_msg.txt_channel, str);
                str = username.c_str();
                strcpy(send_msg.txt_username, str);
                str = text.c_str();
                strcpy(send_msg.txt_text, str);

                send_data = &send_msg;

                len = sizeof send_msg;

                struct sockaddr_in send_sock = channel_user_iter->second;

                bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

                if (bytes < 0)
                {
                    perror("Message failed\n");
                }
            }
        }
        else
        {
            // no users to forward to need to leave if no other servers rely on connection
			if  (is_leaf || (channel_tables[channel]).size() == 1) 
			{ 
				// erase the entry at channel
				channel_tables.erase(channel);
                send_S2S_leave(sock, channel);
			}
        }
	}
	else
	{
		// Duplicate message received send a leave request 
		// to the sender to break the loop.
		send_S2S_leave(sock, channel);
	}
	 
}
