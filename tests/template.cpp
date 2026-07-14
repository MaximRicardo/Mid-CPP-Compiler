// template <template <typename, typename> class C, int x = 123>;

template <typename T, typename A, T tmplt_arg> A func(T arg)
{
    auto x = arg + tmplt_arg;
    return *x;
}
