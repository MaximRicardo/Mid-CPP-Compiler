void func();

namespace std {
float sqrt(float);
}

class A {
public:
    int method()
    {
        return 0;
    }

    int method() const
    {
        return 0;
    }
};

int main()
{
    func();

    std::sqrt(2.f);

    int var;

    A x;
    x.method();

    const A y;
    y.method();
}
