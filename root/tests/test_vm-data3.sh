#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
    sys161 kernel-ASST3 "p uw-testbin/vm-data3;q" >> vm-data3.output 2>/dev/null
    ((counter++))
done

echo $( grep -c 'SUCCEEDED' vm-data3.output ) completed successfully!

rm vm-data3.output

echo test completed
