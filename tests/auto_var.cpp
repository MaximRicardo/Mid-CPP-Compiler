int main()
{
    // i_a is an int
    auto i_a = 0;
    // i_b is a long
    auto i_b = 0L;
    // i_c is an unsigned long
    auto i_c = 0UL;
    // i_d is a long long
    auto i_d = 0LL;
    // i_e is an unsigned long long
    auto i_e = 0ULL;

    // f_a is a float
    auto f_a = 0.f;
    // f_b is a double
    auto f_b = 0.0;
    // f_c is a long double
    auto f_c = 0.L;

    // c_a is a char
    auto c_a = 'x';
    // c_b is a char16_t
    auto c_b = u'x';
    // c_c is a char32_t
    auto c_c = U'x';
    // c_d is a wchar_t
    auto c_d = L'x';

    // s_a is a const char[2]
    auto s_a = "x";
    // s_b is a const char16_t[2]
    auto s_b = u"x";
    // s_c is a const char32_t[2]
    auto s_c = U"x";
    // s_d is a const wchar_t[2]
    auto s_d = L"x";
}
