#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
    sys161 kernel-ASST2 "p uw-testbin/onefork;q" >> onefork-test.output 2>/dev/null
    ((counter++))
done

echo $( grep -c -F 'Operation' onefork-test.output ) completed successfully!

#rm onefork-test.output

echo test completed
