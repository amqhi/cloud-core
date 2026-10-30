#include <jni.h>

#include "core.h"
#include "java_binding_macros.h"
#include "java_item.h"
#include "java_network_provider.h"
#include "java_secure_storage_provider.h"
#include "java_notifier.h"
#include "java_platform_utils.h"
#include "java_uuid.h"

#define JNI_SETTINGS_FUNCTION(type, name) \
extern "C" JNIEXPORT type JNICALL \
Java_com_amqhi_cloud_core_Settings_##name


JNI_SETTINGS_FUNCTION(void, save)(COMMON_PARAMS) {
    CORE(ptr)
    core->settings().save();
}

JNI_SETTINGS_FUNCTION(jstring, getInstanceUrl)(COMMON_PARAMS) {
            CORE(ptr)
            std::string instance_url = core->settings().instance_url();
            return env->NewStringUTF(instance_url.c_str());
}

JNI_SETTINGS_FUNCTION(void, setInstanceUrl)(COMMON_PARAMS,
                                                jstring jInstanceUrl)
{
    CORE(ptr)
    std::string instanceUrl = jstring_to_string(env, jInstanceUrl);

    core->settings().set_instance_url(instanceUrl);
}

JNI_SETTINGS_FUNCTION(jchar, getNavigationBarStyle)(COMMON_PARAMS) {
    CORE(ptr)
    return static_cast<jchar>(core->settings().android_navigation_bar_style());
}

JNI_SETTINGS_FUNCTION(void, setNavigationBarStyle)(COMMON_PARAMS,
                                                jchar style)
{
    CORE(ptr)
    core->settings().set_android_navigation_bar_style(static_cast<char>(style));
}