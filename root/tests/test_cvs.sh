#!/bin/bash


readonly TESTCOUNT=100
echo Attempting to perform $TESTCOUNT tests!
counter=1
while [ $counter -le $TESTCOUNT ]
do
    sys161 kernel-ASST2 "sy3;q" >> cv_test.output 2>/dev/null
    ((counter++))
done

echo $( grep -c 'CV test done' cv_test.output ) completed successfully!

#rm cv_test.output

echo test completed
