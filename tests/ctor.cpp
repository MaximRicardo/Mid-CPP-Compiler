

class A {
public:
    int x, y, z;

    A();
    A(int);
    ~A();
};

namespace NM {
class Test {
public:
    Test();
};
} // namespace NM

int main()
{
    A var = A();
    A var2(10);
    NM::Test x = NM::Test();

    int y(10);
}
