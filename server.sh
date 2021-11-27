#!/bin/bash
# kill $(lsof -t -i:1000)
gcc ./src/server.c -o ./bin/server  
./bin/server
