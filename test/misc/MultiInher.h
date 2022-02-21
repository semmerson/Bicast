class BaseIface
{
public:
    virtual ~BaseIface();

    virtual void foo() =0;
};

class FactoryIface : public BaseIface
{};
