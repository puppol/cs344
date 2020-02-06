#!/bin/bash
sum=0
TMP1=./TMP1
echo -e "8\n7\n6" > $TMP1

while read num
do
   echo "TRACE: In Loop"
   echo "TRACE: num: $num"
   sum=`expr $sum + $num`
   echo "TRACE: sum: $sum"
   echo -e "TRACE: End of Loop\n"
done < $TMP1