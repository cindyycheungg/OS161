#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
        sys161 kernel-ASST2 "p uw-testbin/argtest tests;q" >> argtest.output 2>/dev/null
        ((counter++))
done

echo $( grep -c -F 'Operation' argtest.output ) completed successfully!

#rm argtest.output

echo test completed
