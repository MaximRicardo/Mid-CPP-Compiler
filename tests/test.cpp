char func(int x = 20, float y = 3.141592f)
{
    typedef int i32;
    i32 foo = x / y;
    x / -foo;
}

int main(int argc, char **argv)
{
    char **var = argv + argc + (func(123 / 10, -1.2f) + 0.f);
    bool asd = true;
    int *ptr = nullptr;
}
