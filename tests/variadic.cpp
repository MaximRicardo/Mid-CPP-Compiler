void f(int a, int b, int x, ...);
void f(int a, int b, float x, ...);

void g(int a, int b, int x = 0, ...);
void g(int a, int b, float x = 0.f, ...);

void f(int a = 0, ...);

int main()
{
    f(1, 2, 3);         // calls 1:1
    f(1, 2, 3.f);       // calls 2:1
    f(1, 2, 3, 4, 5);   // calls 1:1
    f(1, 2, 3.f, 4, 5); // calls 2:1

    g(1, 2, 3);         // calls 4:1
    g(1, 2, 3.f);       // calls 5:1
    g(1, 2, 3, 4, 5);   // calls 4:1
    g(1, 2, 3.f, 4, 5); // calls 5:1

    // all these calls go to 7:1
    f();
    f(123);
    f(123.f);
    f(123, 1);
    f(123.f, 1);
}
