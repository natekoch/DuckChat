#!/usr/bin/bash

#uncomment the topolgy you want. The simple two-server topology is uncommented here.

# Change the SERVER variable below to point your server executable.
SERVER=./server

SERVER_NAME=`echo $SERVER | sed 's#.*/\(.*\)#\1#g'`

# Generate a simple two-server topology
$SERVER localhost 8000 localhost 8001 &
$SERVER localhost 8001 localhost 8000 & 

# Generate a capital-H shaped topology
#$SERVER localhost 8000 localhost 8001 &
#$SERVER localhost 8001 localhost 8000 localhost 8002 localhost 8003 &
#$SERVER localhost 8002 localhost 8001 & 
#$SERVER localhost 8003 localhost 8001 localhost 8005 &
#$SERVER localhost 8004 localhost 8005 &
#$SERVER localhost 8005 localhost 8004 localhost 8003 localhost 8006 &
#$SERVER localhost 8006 localhost 8005 &

# Generate a 3x3 grid topologyg
#$SERVER localhost 6000 localhost 6001 localhost 6003 &
#$SERVER localhost 6001 localhost 6000 localhost 6002 localhost 6004 &
#$SERVER localhost 6002 localhost 6001 localhost 6005 &
#$SERVER localhost 6003 localhost 6000 localhost 6004 localhost 6006 &
#$SERVER localhost 6004 localhost 6001 localhost 6003 localhost 6005 localhost 6007 &
#$SERVER localhost 6005 localhost 6002 localhost 6004 localhost 6008 &
#$SERVER localhost 6006 localhost 6003 localhost 6007 &
#$SERVER localhost 6007 localhost 6006 localhost 6004 localhost 6008 &
#$SERVER localhost 6008 localhost 6005 localhost 6007 &


echo "Press ENTER to quit"
read
pkill $SERVER_NAME
