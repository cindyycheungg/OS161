#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
        sys161 kernel-ASST2 "p testbin/add 482 348;q" >> add.output 2>/dev/null
        ((counter++))
done

echo $( grep -c -F 'Operation' add.output ) completed successfully!

#rm add.output

echo test completed
