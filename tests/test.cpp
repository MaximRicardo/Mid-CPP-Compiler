class TestClass {
public:
    int x, y, z;

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

    var.func();
}
