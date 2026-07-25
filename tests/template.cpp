// template <template <typename, typename> class C, int x = 123>;

template <typename T, typename A = int (**)(const char *),
          T tmplt_arg = 123 / 456, int other_tmplt_arg>
A func(T arg)
{
    auto x = arg + tmplt_arg;
    return *x;
}

template <typename T, typename U> class ClassTmplt {
public:
    T memb;
    U &var_a, &&var_b;
};

int main()
{
    ClassTmplt<const char32_t *, unsigned long long> var;
}
