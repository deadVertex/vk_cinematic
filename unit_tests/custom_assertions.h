#pragma once

void AssertWithinVec3(f32 delta, vec3 expected, vec3 actual)
{
    b32 x = Abs(expected.x - actual.x) <= delta;
    b32 y = Abs(expected.y - actual.y) <= delta;
    b32 z = Abs(expected.z - actual.z) <= delta;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Expected [%g, %g, %g] Was [%g, %g, %g].",
            expected.x, expected.y, expected.z, actual.x, actual.y, actual.z);
    TEST_ASSERT_MESSAGE(x && y && z, buffer);
}

void AssertWithinVec4(f32 delta, vec4 expected, vec4 actual)
{
    b32 x = Abs(expected.x - actual.x) <= delta;
    b32 y = Abs(expected.y - actual.y) <= delta;
    b32 z = Abs(expected.z - actual.z) <= delta;
    b32 w = Abs(expected.w - actual.w) <= delta;

    char buffer[256];
    snprintf(buffer, sizeof(buffer),
        "Expected [%g, %g, %g, %g] Was [%g, %g, %g, %g].", expected.x,
        expected.y, expected.z, expected.w, actual.x, actual.y, actual.z,
        actual.w);
    TEST_ASSERT_MESSAGE(x && y && z && w, buffer);
}
