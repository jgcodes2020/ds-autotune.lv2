add_library(ds-autotune SHARED
    "main.cpp"
    "util.hpp"
)
set_target_properties(ds-autotune PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
lv2v_add_ttl_sources(TARGET ds-autotune FILES
    "ds-autotune.ttl"
    "manifest.ttl"
)
target_link_libraries(ds-autotune PRIVATE lvtk::lvtk xtensor xtensor::use_xsimd)

install(TARGETS ds-autotune DESTINATION "lib/lv2/ds-autotune.lv2")
install(FILES "ds-autotune.ttl" "manifest.ttl" DESTINATION "lib/lv2/ds-autotune.lv2")