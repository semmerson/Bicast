class Base {
protected:
    friend class Derived; // Necessary
    Base();
};

class Derived : public Base
{
    Base* foo() {
        return new Base(); // Fails if not friend
    }
};
