#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
readonly testString="p uw-testbin/hogparty;"
STRING=""

i=0
while [[ $i -lt $TESTCOUNT ]] ; do
  STRING="$STRING$testString" 
  (( i += 1 ))
done

sys161 kernel-ASST3 "${STRING} q" >> hogparty-test.output 2>/dev/null

echo $( grep -c -F 'Operation' hogparty-test.output ) completed successfully!

#rm hogparty-test.output

echo test completed
