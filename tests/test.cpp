class TestClass {
public:
    int x, y, z;

    /*
    TestClass foo(int x);

    TestClass func()
    {
        return this->foo(100);
    }
    */

    TestClass func();
};

void func(TestClass);

int sum(int, int);

int main(int argc, char **argv)
{
    TestClass var;
    var.y = 10;
    var.z = 20;
    var.x = var.y + var.z;

    TestClass *ptr;
    auto x = ptr + 10;
}
