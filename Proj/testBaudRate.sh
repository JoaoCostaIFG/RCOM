#!/bin/bash

#function cleanup {
    #echo "killing all childs"
    #pkill -P $$
#}

#trap cleanup EXIT
#declare -a baudRates=(9600 19200 38400 57600 115200 230400 460800)

#for baudRate in ${baudRates[*]}; do
    #./rcom_tp1 -s TRANSMITTER -p 0 -b "$baudRate" -f files/gato.gif > transLog &
    #log=$(./rcom_tp1 -s RECEIVER -p 1 -b "$baudRate")
    #wait
    #echo "$log"
    #sleep 0.5
#done
#!/bin/sh

serial0="10"
serial1="11"

baudrates="9600 19200 38400 57600 115200 230400 460800"

for br in $baudrates; do
  ./rcom_tp1 -s RECEIVER -p "$serial1" -b "$br" 2>&1 > /dev/null &
  p="$!"
  log=$(./rcom_tp1 -s TRANSMITTER -p "$serial0" -b "$br" -f files/gato.gif 2>&1)
  wait "$p"
  echo $log | awk -F'Connection duration: ' '{print $2}' | awk '{print $1}'
done
