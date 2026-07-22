#!/bin/sh
# Define the values of Pakistani currency notes
notes=(10 50 100 500 1000)
# Initialize sum
sum=0
# Iterate over each note and add its value to the sum
for note in "${notes[@]}"; do
  echo "The value of this note is: $note"
  (( sum += note ))
done
# Print the sum of all notes
echo "The sum of all notes is: $sum"
