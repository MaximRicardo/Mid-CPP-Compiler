class A {
public:
    int x;

    int func();
};

int main()
{
    A var;
    (&var)->x = 10;
    var.func();
}
