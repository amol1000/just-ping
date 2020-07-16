#Ping: 
Sends ICMP echo requests to host

    brief: -makes use of raw sockets
           -features count of messages to be sent, by default infinite
           -features interval between echo requests, default is 1 sec
           -features timeout for response, default is 200 

## Usage: 
    sudo ./ping <address/hostname> [-c count] [-i interval(secs)] [-W timeout(mSecs)]

    NOTE: needs root privilages, tested on gcc 7.5.0(ubuntu)

  Author: Amol Kulkarni - amolkulk@andrew.cmu.edu

