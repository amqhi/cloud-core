#include <jni.h>

#include "core.h"
#include "java_binding_macros.h"
#include "map_utils.h"
#include "uuid.h"

#define JNI_CACHED_STATE_FUNCTION(type, name) \
extern "C" JNIEXPORT type JNICALL \
Java_com_amqhi_cloud_core_CachedState_##name

JNI_CACHED_STATE_FUNCTION(jchar, getSortOption)(COMMON_PARAMS, SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(folder_id)
    if (!map_utils::contains_key(core->cached_state().sort_options(), folder_id))
    {
        return sort_option::DEFAULT_OPTION;
    }
    char option = core->cached_state().sort_option(folder_id);
    return option;
}

JNI_CACHED_STATE_FUNCTION(jchar, getViewMode)(COMMON_PARAMS, SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(folder_id)
    if (!map_utils::contains_key(core->cached_state().view_modes(), folder_id))
    {
        return view_mode::DEFAULT_MODE;
    }
    char option = core->cached_state().view_mode(folder_id);
    return option;
}
