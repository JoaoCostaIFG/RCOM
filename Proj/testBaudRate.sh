#!/bin/bash

serial0="10"
serial1="11"

baudrates="9600 19200 38400 57600 115200 230400 460800"

#sshpass -p torvalds ssh -t netedu@192.168.109.13 "mkdir /home/netedu/test"

if [ "$1" == "local" ]; then
    for br in $baudrates; do
      ./rcom_tp1 -s RECEIVER -p "$serial1" -b "$br" 2>&1 &
      p="$!"
      log=$(./rcom_tp1 -s TRANSMITTER -p "$serial0" -b "$br" -f files/gato.gif 2>&1)
      wait "$p"
      echo $log | awk -F'Connection duration: ' '{print $2}' | awk '{print $1}'
    done

elif [ "$1" == "feup" ]; then
    if [ "$2" == "RC" ]; then
        for br in $baudrates; do
            ./rcom_tp1 -s RECEIVER -p "$serial1" -b "$br"

        done
    elif [ "$2" == "TR" ]; then
        echo "" > res
        for br in $baudrates; do
            log=$(./rcom_tp1 -s TRANSMITTER -p "$serial0" -b "$br" -f files/gato.gif 2>&1)
            time=$(echo $log | awk -F'Connection duration: ' '{print $2}' | awk '{print $1}')
            printf "%s\t%s\n" $br $time >> res
        done
    fi
fi

