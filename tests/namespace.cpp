char *x;

namespace A {
int x;

namespace B {
float x;

void f(char *x);
void f(int x);
void f(float x);
} // namespace B
} // namespace A

int main()
{
    A::B::f(x);       // should call line 9
    A::B::f(A::x);    // should call line 10
    A::B::f(A::B::x); // should call line 11
}
