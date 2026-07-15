// template <template <typename, typename> class C, int x = 123>;

template <typename T, typename A = int (**)(const char *),
          T tmplt_arg = 123 / 456, int other_tmplt_arg>
A func(T arg)
{
    auto x = arg + tmplt_arg;
    return *x;
}

template <typename T> class ClassTmplt {
public:
    T memb;
};

template <typename T> return 10;

int main()
{
    func(10);
}
