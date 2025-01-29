echo "Start" > ts.txt
#lst=("1" "2" "4" "8" "12" "24" "48" "480")
for i in "1" "2" "4" "8" "12" "24" "48" "240";
do
  echo "$i 23"
  echo "$i 23" >> ts.txt
  echo "$i 23" | /usr/bin/time ./a.out 2>> ts.txt
done
