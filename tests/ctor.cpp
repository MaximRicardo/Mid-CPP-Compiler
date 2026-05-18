

class A {
public:
    int x, y, z;

    A();
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
    NM::Test x = NM::Test();
}
