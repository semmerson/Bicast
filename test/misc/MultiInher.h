class Iface1
{
public:
    virtual ~Iface1() =default;

    virtual void func1() =0;
};

class Iface2 : public Iface1
{
public:
    virtual ~Iface2() =default;

    virtual void func2() =0;
};
