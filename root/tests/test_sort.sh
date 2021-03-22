#!/bin/bash


readonly TESTCOUNT=30
echo Attempting to perform $TESTCOUNT tests!
readonly testString="p testbin/sort;"
STRING=""

i=0
while [[ $i -lt $TESTCOUNT ]] ; do
  STRING="$STRING$testString" 
  (( i += 1 ))
done

sys161 kernel-ASST3 "${STRING} q" >> sort-test.output 2>/dev/null

echo $( grep -c -F 'Passed' sort-test.output ) completed successfully!

rm sort-test.output

echo test completed
