echo "Start" > ts_small_2.txt
#foo 1
#lst=("1" "2" "4" "8" "12" "24" "48" "480")
for i in "1" "2" "4" "8" "12" "24" "48" "240";
do
  echo "$i 19"
  echo "$i 19" >> ts_small_2.txt
  pids=()
  /usr/bin/time ./foo.sh "$i" 2>> ts_small_2.txt
done
