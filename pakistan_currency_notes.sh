#!/bin/sh
# List of Pakistani currency notes
notes=(10 20 50 100 500 1000)
# Initialize sum
sum=0
# Loop through each note
for note in "${notes[@]}"; do
  # Add note value to sum
  (( sum += note ))
  # Print note value
  echo "$note"
done
# Print total sum
echo "Total: $sum"
