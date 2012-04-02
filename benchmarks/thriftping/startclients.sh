#/bin/bash

if [ $# -ne 5 ]
then
    echo "Usage: startclients.sh num_clients serverIP serverPort myIP number_of_pings"
    exit 0
fi

startport=8000
for i in `seq 1 $1`; do
   port=$(($startport+$i))
   ./nullrpcc -n $5 -i $4 -p $port -r $3 $2 &
done