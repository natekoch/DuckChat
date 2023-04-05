# DuckChat
UO CS 432: Intro to Networks - Programming Assignments 1 &amp; 2 \
Uses the Berkley Sockets API with UDP sockets

## v1
### A client-server chat CLI application

#### Compile with the provided make file.
`$ make all`

#### Usage

NOTE: A server must be launched before clients can join and communicate with each other. 

##### Launching the server 
`$ ./server <server_ip> <server_port>`

##### Launching a client
`$ ./client <server_ip> <server_port> <client_name>`

##### Available commands beyond just sending a message without "/" at the beginning.
* /exit - Logs out the user and quits the client software.
* /join <channel> - Subscribes the client to the channel named <channel>, server will create the channel if it does not exist.
* /leave <channel> - Unsubscribes the client from <channel>.
* /list - Lists the names of all channels created on the server.
* /who <channel> - Lists the names of the other active users on <channel>.
* /switch <channel> - Switches the clients active chat channel to <channel>.

## v2
### A client-server chat CLI application that has server-to-server communication. Servers maintain an active network tree and prunes unecessary connections or connections that cause loops in the network.

#### Compile with the provided make file. 

`$ make all`

#### Usage

##### Launching the servers
Servers can be launched the same as before. \
`$ ./server <server_ip> <server_port>` \
\
To link multiple servers together for server-to-server communication you must specify each neighbor following the server's ip and port. \
`$ ./server <server_ip> <server_port> [[neighbor_ip] [neighbor_port]...]` \
\
The start start_servers.sh shell script will launch a few different server tree configurations that can be commented and uncommented out. 
`$ ./start_servers.sh`

##### Launching a client
Clients may join any of the launched servers and the communication between users will be handled by the connected servers.
`$ ./client <server_ip> <server_port> <client_name>`

All the commands as before in v1 are the same. 

NOTE: The /who command only shows individuals connected to the client's server they connected to. 



