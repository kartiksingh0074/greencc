// GreenCC Test Sample — tests multiple optimization opportunities
#include <iostream>

using namespace std;

int factorial(int n) {
    int result = 1;
    for (int i = 1; i <= n; i++) {
        result = result * i;
    }
    return result;
}

int compute(int size) {
    int total = 0;
    int unused_var = 99;

    for (int i = 0; i < size; i++) {
        // Strength reduction: * 2 -> << 1
        int doubled = i * 2;

        // Constant folding: 5 + 3 + 2
        int bonus = 5 + 3 + 2;

        total = total + doubled + bonus;
    }

    return total;
}

int main() {
    const int N = 5;

    // Constant folding opportunity
    int x = 10 * 20 + 30;

    // Function calls
    int fact = factorial(N);
    int comp = compute(N);

    cout << "Factorial of " << N << " is " << fact << endl;
    cout << "Computed: " << comp << endl;
    cout << "x = " << x << endl;

    // Division by power of 2 -> shift
    int half = x / 2;
    int eighth = x / 8;

    if (half > 100) {
        cout << "Half is large: " << half << endl;
    } else {
        cout << "Half is small: " << half << endl;
    }

    cout << "Eighth: " << eighth << endl;

    return 0;
}
