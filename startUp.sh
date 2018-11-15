#!/bin/bash
make
gnome-terminal --tab -e ./server
gnome-terminal --tab -e "./client gvdambros 1234 /home/gvdambros/test 127.0.0.1 4000" --tab -e "./client gvdambros 1234 /home/gvdambros 127.0.0.1 4000"
read -n 1 -s
killall client
killall server
