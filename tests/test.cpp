int operator-(int x, int y)
{
    0 + x;
}

float &operator-(float x)
{
    0.f - x;
}

char &&operator()(char x, char y)
{
    x + y;
}

int main()
{
    int i_a = 10;
    int i_b = 67;

    float f_a = 3.141592f;

    char c_a = 1;
    char c_b = 2;

    int i_c = i_a - i_b;
    float f_b = -f_a;
    char c_c = c_a(c_b);

    i_a + i_b = 10;
    -f_a = 10.f;
    c_a(c_b) = 10;
}
