// not actually valid c++ but works for testing operator overloading

int operator-(int x, int y) {}

float &operator-(float x) {}

char32_t &&operator()(char32_t x, char32_t y) {}

int main()
{
    int i_a = 10;
    int i_b = 67;

    float f_a = 3.141592f;

    char32_t c_a = 1;
    char32_t c_b = 2;

    int i_c = i_a - i_b;
    float f_b = -f_a;
    char32_t c_c = c_a(c_b);

    // unassignable cuz int operator-(int x, int y) returns a prvalue
    i_a - i_b = 10;
    // assignable cuz float &operator-(float x) returns an lvalue
    -f_a = 10.f;
    // unassignable cuz char32_t &operator()(char32_t x, char32_t y) returns
    // an xvalue
    c_a(c_b) = 10;
}
