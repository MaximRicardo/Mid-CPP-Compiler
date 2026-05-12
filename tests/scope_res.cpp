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

int var = 67;

int main()
{
    f(Class::x);              // should call line 11
    f(Class::NestedClass::y); // should call line 12

    float var = 420;
    f(::var); // should call line 11
    f(var);   // should call line 12
}
