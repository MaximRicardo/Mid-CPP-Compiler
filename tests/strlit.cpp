void str_test()
{
    const char *str_a = "asdf";
    auto str_b = "asdf";

    char c_a = *(str_a + 1);
    char c_b = *(str_a + 2);
}

void wstr_test()
{
    const wchar_t *str_a = L"asdf";
    auto str_b = L"asdf";

    wchar_t c_a = *(str_a + 1);
    wchar_t c_b = *(str_a + 2);
}

void str16_test()
{
    const char16_t *str_a = u"asdf";
    auto str_b = u"asdf";

    char16_t c_a = *(str_a + 1);
    char16_t c_b = *(str_a + 2);
}

void str32_test()
{
    const char32_t *str_a = U"asdf";
    auto str_b = "asdf";

    char32_t c_a = *(str_a + 1);
    char32_t c_b = *(str_a + 2);
}

void f(const char *str);
void f(const wchar_t *str);
void f(const char16_t *str);
void f(const char32_t *str);

int main()
{
    str_test();
    wstr_test();
    str16_test();
    str32_test();

    f("123");  // should call 37:1
    f(L"123"); // should call 38:1
    f(u"123"); // should call 39:1
    f(U"123"); // should call 40:1

    auto str = "123";
    auto wstr = L"123β";
    auto str16 = u"123猫";
    auto str32 = U"123🥀";

    f(str);   // should call 37:1
    f(wstr);  // should call 38:1
    f(str16); // should call 39:1
    f(str32); // should call 40:1
}
