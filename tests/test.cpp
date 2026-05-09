class Vec3;

class Vec3 {
public:
    float x;
    float y;
    float z;

    Vec3 operator*(float scale);
    Vec3 operator*(const Vec3 &);
    Vec3 operator+(const Vec3 &);
};

class Vec3;

void func(int x);
void func(int x, int y);
void func(int x, float y);

void f(int x, int y, int z);
void f(int x, int y = 2, int z);
void f(int x = 3, int y = 2, int z);

int main()
{
    Vec3 x;
    Vec3 y;
    auto z = x * 10;
    auto w = x * y;
    auto a = x + y;

    func(10);
    func(10, 20.f);
    func(10, 20);
}
