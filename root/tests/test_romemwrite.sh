#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
    sys161 kernel-ASST3 "p uw-testbin/romemwrite;q" >> romemwrite.output 2>/dev/null
    ((counter++))
done

echo $( grep -c 'Operation' romemwrite.output ) completed successfully!

#rm romemwrite.output

echo test completed
