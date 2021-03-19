#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
    sys161 kernel-ASST2 "sy2;q" >> lock_test.output 2>/dev/null
    ((counter++))
done

echo $( grep -c 'Lock test done' lock_test.output ) completed successfully!

#rm lock_test.output

echo test completed
