class TestClass {
public:
    int x, y, z;

    TestClass(int x, int y, int z);

    TestClass foo(int x);

    TestClass func()
    {
        return this->foo(100);
    }
};

void func();

int main(int argc, char **argv)
{
    TestClass var(argc, argc, argc);
    var = TestClass(1, 2, 3);
}
