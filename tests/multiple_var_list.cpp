void func()
{
    int a, b, c;
    char d, *e, **f;
    char g, *const h = nullptr, **const i = nullptr;
    int j = 0, k = 1, l = 123.L;
}

class A {
    int x, y, z;
} A_a, A_b, A_c;

const class B {
    int x, y, z;
} *B_a, **B_b, ***B_c;

class C {
public:
    int x, y, z;
} *const C_a = nullptr, **const C_b = nullptr, ***const C_c = nullptr;

void func2()
{
    C_a->x = 10;
}
