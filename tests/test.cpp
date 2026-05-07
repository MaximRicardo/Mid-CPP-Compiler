float operator+(float a, int b);

class Test {

    int x = y;
    int y;

public:
    int func(int arg)
    {
        z + arg / (x - y);
    }

    float z;
};

Test operator*(const Test &a, int scale);

void func(int x);
void func(int x, int y);
void func(int x, float y);

int main()
{
    Test x;
    x * 10;

    func(10);
    func(10, 20.f);
    func(10, 20);
}
