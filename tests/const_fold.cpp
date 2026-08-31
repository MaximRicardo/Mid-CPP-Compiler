void func()
{
    constexpr static int x = 10;
    constexpr const int *a = &x;
    constexpr const int *b = a + 1;
    constexpr int d_a = *a;

    constexpr const char *str = "asdf";
    constexpr const char *str_p0 = str + 1;
    constexpr const char *str_p1 = str + 2;
    constexpr const char *str_p2 = str + 3;
    constexpr const char *str_p3 = str + 4;
    constexpr const char *str_p4 = str + 5;

    // TODO: get this to work cuz rn the expression evaluator doesn't default
    //       construct automatically
    /*
    class Class {
        int x = 0, y = 1, z = 2;
    };

    constexpr static Class class_var;
    constexpr const Class *class_p0 = &class_var;
    constexpr const Class *class_p1 = &class_var + 1;
    */
}
