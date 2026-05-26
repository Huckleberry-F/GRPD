#include "MathUtils.h"
#include <iostream>
#include <cmath>

#define test_assert(cond) \
    if (!(cond)) { \
        std::cerr << "[ERROR] Assertion failed at line " << __LINE__ << ": " << #cond << std::endl; \
        return 1; \
    }

int main() {
    std::cout << "[test_math_utils] Starting TDD test..." << std::endl;
    
    // Test 1: Normal division
    test_assert(std::abs(PDCommon::Utils::safeDivide(10.0, 2.0) - 5.0) < 1e-15);
    
    // Test 2: Exactly zero division (using default fallback = 0.0)
    test_assert(PDCommon::Utils::safeDivide(10.0, 0.0) == 0.0);
    
    // Test 3: Exactly zero division (using custom fallback = 99.0)
    test_assert(PDCommon::Utils::safeDivide(10.0, 0.0, 99.0) == 99.0);
    
    // Test 4: Epsilon trigger (denominator absolute value < epsilon)
    test_assert(PDCommon::Utils::safeDivide(10.0, 1e-16, 99.0, 1e-15) == 99.0);
    
    // Test 5: Safe execution just above epsilon
    test_assert(std::abs(PDCommon::Utils::safeDivide(10.0, 1e-14, 99.0, 1e-15) - 1e15) < 1e-15);
    
    std::cout << "[test_math_utils] All tests PASSED!" << std::endl;
    return 0;
}
