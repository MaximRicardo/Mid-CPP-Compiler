// template <template <typename, typename> class C, int x = 123>;

template <typename T, T tmplt_arg> int func(T arg)
{
    auto x = tmplt_arg + (arg - "asdf");
    (x + 10) - L"asdf";
}
