void func()
{
    constexpr int x = 10 * 24;
    constexpr int y = x * x;
    constexpr int z = y + 10;

    float *a[10];
    const int *const(*const(*b))[z];
}
