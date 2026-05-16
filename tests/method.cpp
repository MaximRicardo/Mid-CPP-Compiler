class A {
    int x;
    char *str;

public:
    void method(int arg)
    {
        char c = *(str + arg);
        x += c;
    }
};

int main()
{
    A var;
    var.method(123);
}
