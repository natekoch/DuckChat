#!/usr/bin/bash

#uncomment the topolgy you want. The simple two-server topology is uncommented here.

# Change the SERVER variable below to point your server executable.
SERVER=./server

SERVER_NAME=`echo $SERVER | sed 's#.*/\(.*\)#\1#g'`

VALGRIND=`echo memcheck-amd64- | sed 's#.*/\(.*\)#\1#g'`

# Generate a simple two-server topology
#$SERVER localhost 4000 localhost 4001 &
#$SERVER localhost 4001 localhost 4000 & 

# Generate a capital-H shaped topology
#$SERVER localhost 6000 localhost 6001 &
#$SERVER localhost 6001 localhost 6000 localhost 6002 localhost 6003 &
#$SERVER localhost 6002 localhost 6001 & 
#$SERVER localhost 6003 localhost 6001 localhost 6005 &
#$SERVER localhost 6004 localhost 6005 &
#$SERVER localhost 6005 localhost 6004 localhost 6003 localhost 6006 &
#$SERVER localhost 6006 localhost 6005 &

# Generate a 3x3 grid topologyg
valgrind $SERVER localhost 6000 localhost 6001 localhost 6003 &
$SERVER localhost 6001 localhost 6000 localhost 6002 localhost 6004 &
$SERVER localhost 6002 localhost 6001 localhost 6005 &
$SERVER localhost 6003 localhost 6000 localhost 6004 localhost 6006 &
$SERVER localhost 6004 localhost 6001 localhost 6003 localhost 6005 localhost 6007 &
$SERVER localhost 6005 localhost 6002 localhost 6004 localhost 6008 &
$SERVER localhost 6006 localhost 6003 localhost 6007 &
$SERVER localhost 6007 localhost 6006 localhost 6004 localhost 6008 &
$SERVER localhost 6008 localhost 6005 localhost 6007 &


echo "Press ENTER to quit"
read
pkill $SERVER_NAME
pkill $VALGRIND
