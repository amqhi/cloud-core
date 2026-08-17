set(CLOUD_INCLUDE_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/src/models
        ${CMAKE_CURRENT_LIST_DIR}/src/utils
)

set(CLOUD_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/core.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/core.h
        ${CMAKE_CURRENT_LIST_DIR}/src/settings.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/settings.h
        ${CMAKE_CURRENT_LIST_DIR}/src/item_manager.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/item_manager.h
        ${CMAKE_CURRENT_LIST_DIR}/src/cached_state.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/cached_state.h
        ${CMAKE_CURRENT_LIST_DIR}/src/database_provider.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/database_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/src/secure_storage_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/src/platform_utils.h
        ${CMAKE_CURRENT_LIST_DIR}/src/api.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/api.h
        ${CMAKE_CURRENT_LIST_DIR}/src/utils/item_utils.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/utils/item_utils.h
)

set(CLOUD_JAVA_BINDING_INCLUDE_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/java
)

set(CLOUD_JAVA_BINDING_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/java/java_binding.cpp
        ${CMAKE_CURRENT_LIST_DIR}/java/java_network_provider.h
        ${CMAKE_CURRENT_LIST_DIR}/java/java_notifier.h
        ${CMAKE_CURRENT_LIST_DIR}/java/java_platform_utils.h
        ${CMAKE_CURRENT_LIST_DIR}/java/java_secure_storage_provider.h
)