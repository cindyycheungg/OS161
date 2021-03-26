#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
readonly testString="p uw-testbin/widefork;"
STRING=""

i=0
while [[ $i -lt $TESTCOUNT ]] ; do
  STRING="$STRING$testString" 
  (( i += 1 ))
done

sys161 kernel-ASST3 "${STRING} q" >> widefork-test.output 2>/dev/null

echo $( grep -c -F 'Operation' widefork-test.output ) completed successfully!

#rm widefork-test.output

echo test completed
