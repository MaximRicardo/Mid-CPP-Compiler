int operator-(int x, int y)
{
    0 + x;
}

// c++ style comment test

float &operator-(float x)
{
    0.f - x;
}

char32_t &&operator()(char32_t x, char32_t y)
{
    x + y;
}

/*
 * c style comment test
 * blah blah blah
 * blah blah blah
 * blah blah blah
 * blah blah blah
 */

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

    i_a + i_b = 10;
    -f_a = 10.f;
    c_a(c_b) = 10;
}
