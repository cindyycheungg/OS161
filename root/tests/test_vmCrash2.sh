#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
    sys161 kernel-ASST3 "p uw-testbin/vm-crash2;q" >> vm-crash2.output 2>/dev/null
    ((counter++))
done

echo $( grep -c 'Operation' vm-crash2.output ) completed successfully!

#rm vm-crash2.output

echo test completed
