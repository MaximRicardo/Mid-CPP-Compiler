class TestClass {
public:
    int x, y, z;

    TestClass func();
};

void func(TestClass);

int main(int argc, char **argv)
{
    TestClass var;
    var.func().func();
}
