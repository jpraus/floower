
#include <Arduino.h>
#include <unity.h>
#include "behavior/SmartPowerBehavior.h""

SmartPowerBehavior *smartPowerBehavior;

void test_example(void) {
    TEST_ASSERT_EQUAL(32, 32);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    UNITY_END();

    return 0;
}