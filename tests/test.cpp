int func(int &arg)
{
    arg = -67;

    return arg;
}

int main(int argc, char **argv)
{
    int x = 5;
    func(x);
}
