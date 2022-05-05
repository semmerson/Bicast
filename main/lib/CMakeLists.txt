add_library(hycast
        $<TARGET_OBJECTS:misc>
        $<TARGET_OBJECTS:inet>
        $<TARGET_OBJECTS:p2p>
        $<TARGET_OBJECTS:mcast>
        $<TARGET_OBJECTS:repository>
        $<TARGET_OBJECTS:node>
        $<TARGET_OBJECTS:main>
)
#       $<TARGET_OBJECTS:protocol>

# "PUBLIC" enables clients of "hycast" to obtain correct SSL library
#target_link_options(hycast PUBLIC -L/usr/lib64/openssl11)
#target_link_libraries(hycast yaml-cpp ssl pthread)
target_link_libraries(hycast yaml-cpp pthread)

install(TARGETS hycast DESTINATION lib)
