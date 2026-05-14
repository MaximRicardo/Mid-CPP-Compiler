typedef float Type;

class Class {
public:
    static int x;

    class NestedClass {
    public:
        static float y;
    };

    typedef int Type;
};

void f(int x);
void f(float x);

int var = 67;

int main()
{
    f(Class::x);              // should call line 15
    f(Class::NestedClass::y); // should call line 16

    float var = 420;
    f(::var); // should call line 15
    f(var);   // should call line 16

    Type a = 5.f;
    Class::Type b = 5;
    f(a); // should call line 16
    f(b); // should call line 15
}
