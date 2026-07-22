#!/bin/bash
read -p "Enter first number: " num1
read -p "Enter second number: " num2
read -p "Enter third number: " num3
read -p "Enter fourth number: " num4
read -p "Enter fifth number: " num5

sum=$((num1 + num2 + num3 + num4 + num5))

if (( num1 >= num2 && num1 >= num3 && num1 >= num4 && num1 >= num5 )); then
  max=$num1
elif (( num2 >= num1 && num2 >= num3 && num2 >= num4 && num2 >= num5 )); then
  max=$num2
elif (( num3 >= num1 && num3 >= num2 && num3 >= num4 && num3 >= num5 )); then
  max=$num3
elif (( num4 >= num1 && num4 >= num2 && num4 >= num3 && num4 >= num5 )); then
  max=$num4
else
  max=$num5
fi

if (( num1 <= num2 && num1 <= num3 && num1 <= num4 && num1 <= num5 )); then
  min=$num1
elif (( num2 <= num1 && num2 <= num3 && num2 <= num4 && num2 <= num5 )); then
  min=$num2
elif (( num3 <= num1 && num3 <= num2 && num3 <= num4 && num3 <= num5 )); then
  min=$num3
elif (( num4 <= num1 && num4 <= num2 && num4 <= num3 && num4 <= num5 )); then
  min=$num4
else
  min=$num5
fi

echo "Numbers are: $num1 $num2 $num3 $num4 $num5"
echo "Sum of numbers: $sum"
echo "Largest number: $max"
echo "Smallest number: $min"
echo "Odd and Even numbers:"
for num in $num1 $num2 $num3 $num4 $num5; do
  if (( num % 2 == 0 )); then
    echo "$num is even"
  else
    echo "$num is odd"
  fi
done
