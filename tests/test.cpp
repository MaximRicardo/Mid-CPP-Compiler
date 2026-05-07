class Vec3 {
public:
    float x;
    float y;
    float z;

    Vec3 operator*(float scale);
    Vec3 operator*(const Vec3 &);
};

void func(int x);
void func(int x, int y);
void func(int x, float y);

int main()
{
    Vec3 x;
    Vec3 y;
    auto z = x * 10;
    auto w = x * y;

    func(10);
    func(10, 20.f);
    func(10, 20);
}
