class Vec3 {
public:
    float x;
    float y;
    float z;

    Vec3 operator+(const Vec3 &);
    Vec3 operator-(const Vec3 &);
    Vec3 operator*(float);
    Vec3 operator*(const Vec3 &);
    Vec3 operator/(float);
    Vec3 operator/(const Vec3 &);

    float dot(const Vec3 &);
    Vec3 cross(const Vec3 &);
    float len();
    Vec3 normalized();

    Vec3 &operator+=(const Vec3 &);
    Vec3 &operator-=(const Vec3 &);
    Vec3 &operator*=(float);
    Vec3 &operator*=(const Vec3 &);
    Vec3 &operator/=(float);
    Vec3 &operator/=(const Vec3 &);
};

int main()
{
    Vec3 a;
    Vec3 b;
    Vec3 c;

    c = a + b;     // calls operator+(const Vec3 &)
    c = a - b;     // calls operator-(const Vec3 &)
    c = a * 123;   // calls operator*(float)
    c = a * b;     // calls operator*(const Vec3 &)
    c = a / 123.f; // calls operator/(float)
    c = a / b;     // calls operator/(const Vec3 &)

    c += a;      // calls operator+=(const Vec3 &)
    c -= a;      // calls operator-=(const Vec3 &)
    c *= 123ULL; // calls operator*=(float)
    c *= a;      // calls operator*=(const Vec3 &)
    c /= 123.L;  // calls operator/=(float)
    c /= a;      // calls operator/=(const Vec3 &)
}
