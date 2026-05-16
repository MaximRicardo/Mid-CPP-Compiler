class A {
    int x;
    char *str;

public:
    char method(int arg)
    {
        char c = *(str + arg);
        x += c;
        return c;
    }
};

int main()
{
    A var;
    var.method(123);
}
