#include <memory>

template<typename T>
class FactoryIface
{
public:
    using Pimpl = std::shared_ptr<FactoryIface>;

    static Pimpl create();

    T func();
};
