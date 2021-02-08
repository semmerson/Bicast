add_library(hycast
        $<TARGET_OBJECTS:misc>
        $<TARGET_OBJECTS:inet>
        $<TARGET_OBJECTS:protocol>
        $<TARGET_OBJECTS:p2p>
        $<TARGET_OBJECTS:repository>
        $<TARGET_OBJECTS:node>
)

install(TARGETS hycast DESTINATION lib)