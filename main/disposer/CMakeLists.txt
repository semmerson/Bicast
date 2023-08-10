# Add the library
add_library(disposer OBJECT
        Disposer.h
        Action.cpp              Action.h
                                PatternAction.h
        ActionTemplate.cpp      ActionTemplate.h
        Disposer.cpp            Disposer.h
)

# Add the product-diposition example
install(FILES dispose_example.yaml DESTINATION etc)