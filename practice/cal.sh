#!/bin/bash
echo "Simple Calculator"
echo "1. Addition"
echo "2. Subtraction"
echo "3. Multiplication"
echo "4. Division"
read -p "Enter your choice (1-4): " choice
read -p "Enter first number: " num1
read -p "Enter second number: " num2

case $choice in
  1) echo "Result: $num1 + $num2 = $(($num1 + $num2))";;
  2) echo "Result: $num1 - $num2 = $(($num1 - $num2))";;
  3) echo "Result: $num1 * $num2 = $(($num1 * $num2))";;
  4) if [ $num2 -ne 0 ]; then
       echo "Result: $num1 / $num2 = $(($num1 / $num2))"
     else
       echo "Error: Division by zero is not allowed."
     fi;;
  *) echo "Invalid choice";;
esac
EOF 
bash cal.sh
pwd > /tmp/synapse_pwd_483.txt