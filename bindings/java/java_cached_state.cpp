#include <jni.h>

#include "core.h"
#include "java_binding_macros.h"

#define JNI_CACHED_STATE_FUNCTION(type, name) \
extern "C" JNIEXPORT type JNICALL \
Java_com_amqhi_cloud_core_CachedState_##name

JNI_CACHED_STATE_FUNCTION(void, save)(COMMON_PARAMS)
{
    CORE(ptr)
    core->cached_state().save();
}

JNI_CACHED_STATE_FUNCTION(jlong, getLastUpdateCheck)(COMMON_PARAMS)
{
    CORE(ptr)
    return static_cast<jlong>(core->cached_state().last_update_check());
}

JNI_CACHED_STATE_FUNCTION(void, updateLastUpdateCheck)(COMMON_PARAMS)
{
    CORE(ptr)
    core->cached_state().update_last_update_check();
}

