class Class {
public:
    int x = 10;
    float *y;
    class Nested {
        int nested_x = 123;
        long double nested_y = 10.0l / 3.0l;
        unsigned long long nested_z;

    public:
        constexpr Nested() : nested_z() {}
    } z;

    constexpr Class() : y() {}
};

/*
float var = 123 / 345.234;

constexpr int func(int x)
{
    typedef class Name {
        int x;
    } name;
    return x;
}
*/
