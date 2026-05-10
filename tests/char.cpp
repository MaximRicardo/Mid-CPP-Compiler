int main()
{
    char a = 'x';
    char16_t b = u'猫';
    char32_t c = U'🥀';
    wchar_t d = L'β';

    // should error out for being too big
    char err_a = '猫';
    char16_t err_b = u'🥀';
}
