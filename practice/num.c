#include <stdio.h>

int main() {
    int numbers[5];
    int sum = 0, max, min, odd_count = 0, even_count = 0;
    printf("Enter 5 numbers: \n");

    for (int i = 0; i < 5; i++) {
        scanf("%d", &numbers[i]);
        sum += numbers[i];
    }

    max = numbers[0];
    min = numbers[0];

    for (int i = 1; i < 5; i++) {
        if (numbers[i] > max) {
            max = numbers[i];
        }
        if (numbers[i] < min) {
            min = numbers[i];
        }
    }

    for (int i = 0; i < 5; i++) {
        if (numbers[i] % 2 == 0) {
            even_count++;
        } else {
            odd_count++;
        }
    }

    printf("Sum: %d\n", sum);
    printf("Max: %d\n", max);
    printf("Min: %d\n", min);
    printf("Odd count: %d\n", odd_count);
    printf("Even count: %d\n", even_count);

    return 0;
}
