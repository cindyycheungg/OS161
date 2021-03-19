#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
        sys161 kernel-ASST2 "p uw-testbin/hogparty;q" >> hogparty-test.output 2>/dev/null
        ((counter++))
done

echo $( grep -c -F 'Operation' hogparty-test.output ) completed successfully!

#rm hogparty-test.output

echo test completed
