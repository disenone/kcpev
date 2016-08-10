#!/bin/sh
if [ $# != 2 ]
then
    echo "usage: pressure_test ip port"
    exit 1
fi

echo "Running pressure tests"

for i in {1..100}
do
	echo $i
    (./kcpev_client_pressure_test $1 $2 &)
    sleep 0.001
done 
