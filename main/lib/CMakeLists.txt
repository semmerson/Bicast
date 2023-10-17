add_library(bicast
        $<TARGET_OBJECTS:misc>
        $<TARGET_OBJECTS:inet>
        $<TARGET_OBJECTS:p2p>
        $<TARGET_OBJECTS:mcast>
        $<TARGET_OBJECTS:repository>
        $<TARGET_OBJECTS:disposer>
        $<TARGET_OBJECTS:node>
        $<TARGET_OBJECTS:main>
)
#       $<TARGET_OBJECTS:protocol>

# Neither "-lyaml-cpp.so" nor "-lyaml-cpp.so.0" records the right YAML library
target_link_libraries(bicast PUBLIC -lyaml-cpp ssl pthread)

install(TARGETS bicast DESTINATION lib)
