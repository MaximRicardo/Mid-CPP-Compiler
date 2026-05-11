class Class {
public:
    static int x;

    class NestedClass {
    public:
        static float y;
    };
};

void f(int x);
void f(float x);

int main()
{
    f(Class::x);
    f(Class::NestedClass::y);
}
