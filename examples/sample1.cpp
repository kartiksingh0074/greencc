// GreenCC Sample Input — exercises optimization-friendly patterns
#include <iostream>

using namespace std;

// Compute a value with energy-wasteful patterns
int computeValue(int size) {
    int sum = 0;
    int zero = 0;
    int unused = 42;

    for (int i = 0; i < size; i++) {
        // Constant expression that could be folded
        int multiplier = 2 * 3 * 4;

        // Strength reduction candidate: x * 8 → x << 3
        int scaled = i * 8;

        // Redundant add with zero
        int offset = i + zero;

        sum = sum + scaled + multiplier;
    }

    return sum;
}

// Demonstrate branching and I/O
int main() {
    const int MAX = 10;

    int result = computeValue(MAX);

    if (result > 100) {
        std::cout << "Large result: " << result << std::endl;
    } else {
        std::cout << "Small result: " << result << std::endl;
    }

    // Energy-wasteful: divide where shift would work
    int half = result / 2;
    int quarter = result / 4;

    // Constant expression
    int fixed = 10 + 20 + 30;

    std::cout << "Half: " << half << std::endl;
    std::cout << "Quarter: " << quarter << std::endl;
    std::cout << "Fixed: " << fixed << std::endl;

    return 0;
}
