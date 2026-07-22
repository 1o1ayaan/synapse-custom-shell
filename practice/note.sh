#!/bin/bash
notes=(10 50 100 500 1000 5000)
sum=0
echo "Pakistani Notes:"
for note in "${notes[@]}"; do
  echo "$note"
  ((sum += note))
done
echo -e "\nSum of Pakistani Notes: $sum" > note.txt
echo "Sum of notes has been written to note.txt"
