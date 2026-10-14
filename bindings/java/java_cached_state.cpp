#include <jni.h>

#include "core.h"
#include "java_binding_macros.h"
#include "map_utils.h"
#include "uuid.h"

#define JNI_CACHED_STATE_FUNCTION(type, name) \
extern "C" JNIEXPORT type JNICALL \
Java_com_amqhi_cloud_core_CachedState_##name

JNI_CACHED_STATE_FUNCTION(jchar, getSortOption)(COMMON_PARAMS, UUID_PARAMS)
{
    CORE(ptr)
    UUID_FROM_PARAMS(folder_id);
    if (!map_utils::contains_key(core->cached_state().get().sort_options, folder_id))
    {
        return sort_option::DEFAULT_OPTION;
    }
    char option = core->cached_state().get().sort_options[folder_id];
    return option;
}

JNI_CACHED_STATE_FUNCTION(jchar, getViewMode)(COMMON_PARAMS, UUID_PARAMS)
{
    CORE(ptr)
    UUID_FROM_PARAMS(folder_id);
    if (!map_utils::contains_key(core->cached_state().get().view_modes, folder_id))
    {
        return view_mode::DEFAULT_MODE;
    }
    char option = core->cached_state().get().view_modes[folder_id];
    return option;
}
