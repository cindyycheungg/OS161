#!/bin/bash


readonly TESTCOUNT=30
echo Attempting to perform $TESTCOUNT tests!
readonly testString="p uw-testbin/vm-data1;"
STRING=""

i=0
while [[ $i -lt $TESTCOUNT ]] ; do
  STRING="$STRING$testString" 
  (( i += 1 ))
done

sys161 kernel-ASST3 "${STRING} q" >> vm-data1-test.output 2>/dev/null

echo $( grep -c -F 'SUCCEEDED' vm-data1-test.output ) completed successfully!

#rm vm-data1-test.output

echo test completed
