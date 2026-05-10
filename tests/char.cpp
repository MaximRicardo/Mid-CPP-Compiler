void f(char c);
void f(wchar_t c);
void f(char16_t c);
void f(char32_t c);

int main()
{
    char a = 'x';
    char16_t b = u'猫';
    char32_t c = U'🥀';
    wchar_t d = L'β';

    f('a');  // calls 1:1
    f(L'a'); // calls 2:1
    f(u'a'); // calls 3:1
    f(U'a'); // calls 4:1

    f(a); // calls 1:1
    f(b); // calls 2:1
    f(c); // calls 3:1
    f(d); // calls 4:1
}
