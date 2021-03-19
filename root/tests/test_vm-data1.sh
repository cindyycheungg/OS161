#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
    sys161 kernel-ASST3 "p uw-testbin/vm-data1;q" >> vm-data1.output 2>/dev/null
    ((counter++))
done

echo $( grep -c 'SUCCEEDED' vm-data1.output ) completed successfully!

rm vm-data1.output

echo test completed
