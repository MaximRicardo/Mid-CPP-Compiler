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
};

int main()
{
    func();

    std::sqrt(2.f);

    A var;
    var.method();
}
