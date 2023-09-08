class BaseIface
{
public:
    virtual ~BaseIface() =default;

    virtual void func() const =0;
};

class DerivedIface : virtual public BaseIface
{
public:
    virtual ~DerivedIface() =default;
};
