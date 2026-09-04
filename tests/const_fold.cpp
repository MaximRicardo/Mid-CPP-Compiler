void func()
{
    constexpr static int x = 10;
    constexpr const int *a = &x;
    constexpr const int *b = a + 1;
    constexpr int d_a = *a;

    constexpr const char *str = "asdf";
    constexpr const char *str_p0a = str + 0;
    constexpr const char *str_p1a = str + 1;
    constexpr const char *str_p2a = str + 2;
    constexpr const char *str_p3a = str + 3;
    constexpr const char *str_p4a = str + 4;
    constexpr const char *str_p5a = str + 5;
    constexpr char str_c0 = str[0];
    constexpr char str_c1 = str[1];
    constexpr char str_c2 = str[2];
    constexpr char str_c3 = str[3];
    constexpr char str_c4 = str[4];
    constexpr const char *str_p0b = &str[0];
    constexpr const char *str_p1b = &str[1];
    constexpr const char *str_p2b = &str[2];
    constexpr const char *str_p3b = &str[3];
    constexpr const char *str_p4b = &str[4];
    constexpr const char *str_p5b = &str[5];

    class Class {
    public:
        int x = 0, y = 1, z = 2;
    };

    constexpr static Class class_var;
    constexpr const Class *class_p0 = &class_var;
    constexpr const Class *class_p1 = &class_var + 1;

    constexpr auto field = class_var.z + class_var.y;
    constexpr auto p_field = class_p0->z + class_p0->y;

    constexpr long double *flt_p = nullptr;
}
