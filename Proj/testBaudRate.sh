#!/bin/bash

serial0="0"
serial1="0"

baudrates="9600 19200 38400 57600 115200" # 230400 460800"

#sshpass -p torvalds ssh -t netedu@192.168.109.13 "mkdir /home/netedu/test"

if [ "$1" == "local" ]; then
    for br in $baudrates; do
      ./rcom_tp1 -s RECEIVER -p "$serial1" -b "$br" 2>&1 &
      p="$!"
      log=$(./rcom_tp1 -s TRANSMITTER -p "$serial0" -b "$br" -f files/pinguim.gif 2>&1)
      wait "$p"
      echo $log | awk -F'Connection duration: ' '{print $2}' | awk '{print $1}'
    done

elif [ "$1" == "feup" ]; then
    if [ "$2" == "RC" ]; then
        for br in $baudrates; do
            ./rcom_tp1 -s RECEIVER -p "$serial1" -b "$br"
	    sleep 1
        done
    elif [ "$2" == "TR" ]; then
	exec 5>&1
        echo "" > res
        for br in $baudrates; do
            log=$(./rcom_tp1 -s TRANSMITTER -p "$serial0" -b "$br" -f files/pinguim.gif 2>&1 | tee >(cat - >&5))
            time=$(echo $log | awk -F'Connection duration: ' '{print $2}' | awk '{print $1}')
            sent=$(echo $log | awk -F'Number of frames sent: ' '{print $2}' | awk '{print $1}')
            resent=$(echo $log | awk -F'Number of frames re-sent: ' '{print $2}' | awk '{print $1}')
            printf "%s\t%s\t%d\t%d\n" $br $time $sent $resent >> res
	    sleep 1
        done
    fi
fi

