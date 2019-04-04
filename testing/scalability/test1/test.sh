#! /bin/bash

TEST_FILE1=test1.xml
TEST_FILE2=test2.xml
TEST_FILE3=test3.xml

# change this number to adjust the request sent
NUM_REQUESTS=0

./test $TEST_FILE1

for ((i = 0; i < $NUM_REQUESTS; ++i))
do
	./test $TEST_FILE2 &
	./test $TEST_FILE3 &
done

