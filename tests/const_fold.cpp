class Class {
public:
    int x = 10;
    float *y;
    class Nested {
        int nested_x = 123;
        long double nested_y = 10.0l / 3.0l;
        unsigned long long nested_z;
        static int asdf;
        volatile int evil_member_that_makes_this_class_non_literal;

    public:
        constexpr Nested() : nested_z(), asdf(10) {}
    } z;

    constexpr Class() : y() {}
};

void func() : a(10) {}

constexpr int func(int x)
{
    // fails due to having a declaration inside in a constexpr func body
    int y = x;
    return y;
}

constexpr float func(float x)
{
    // fails due to multiple returns
    return x;
    return x;
}

constexpr Class var; // fails cuz Class isn't a literal type

constexpr class DefaultConstructibleClass {
protected:
    int x = 10 * 2;

public:
    float y = 6 + 7.f;
    void *z;

    constexpr DefaultConstructibleClass() : z() {}
} other_var; // default constructible so doesn't have to be init-ed

constexpr class NonDefaultConstructibleClass {
    int x;

public:
    constexpr NonDefaultConstructibleClass(int) : x() {}
} foo; // not default constructible so needs to be init-ed
