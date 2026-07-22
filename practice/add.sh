notes=(10 50 100 500 1000)
sum=0
for note in "${notes[@]}"; do
  read -p "Enter number of Rs $note notes: " count
  sum=$((sum + note * count))
done
echo "Total amount: Rs $sum"
