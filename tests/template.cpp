// template <template <typename, typename> class C, int x = 123>;

template <typename T, int tmplt_arg> int func(T arg)
{
    tmplt_arg = 10;
}
