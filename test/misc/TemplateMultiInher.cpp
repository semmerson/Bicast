#include "TemplateMultiInher.h"

template<typename T>
class FactoryImpl : public FactoryIface<T>
{
public:
    T func() {}
};

template<>
FactoryIface<int>::Pimpl FactoryIface<int>::create() {
    return Pimpl{new FactoryImpl<int>()};
}
