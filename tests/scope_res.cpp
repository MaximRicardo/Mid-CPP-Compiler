void f(int x);
void f(float x);

typedef float Type;

class Class {
public:
    static int x;

    class NestedClass {
    public:
        static float y;
    };

    class NestedClass2;

    typedef int Type;
};

class Class::NestedClass2 {
protected:
    float memb;
};

int var = 67;

int main()
{
    f(Class::x);              // should call line 1
    f(Class::NestedClass::y); // should call line 2

    float var = 420;
    f(::var); // should call line 1
    f(var);   // should call line 2

    Type a = 5.f;
    Class::Type b = 5;
    f(a); // should call line 2
    f(b); // should call line 1

    Class::NestedClass2 c_nc2_instance;
}
