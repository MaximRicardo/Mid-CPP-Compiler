class A {
public:
    int x;

    void func();
};

int main()
{
    A var;
    (&var)->x = 10;
    var.func();
}
