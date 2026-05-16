void f(int);

namespace NM_A {
class C_A {
public:
    int x;
};

void f(C_A);
} // namespace NM_A

int main()
{
    f(5);

    NM_A::C_A c_a;
    f(c_a);
}
