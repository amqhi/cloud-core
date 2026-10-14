#include <jni.h>
#include <java_user.h>

#include "core.h"
#include "java_network_provider.h"
#include "java_secure_storage_provider.h"
#include "java_notifier.h"
#include "java_platform_utils.h"
#include "java_binding_macros.h"

#define JNI_CORE_FUNCTION(type, name) \
extern "C" JNIEXPORT type JNICALL \
Java_com_amqhi_cloud_core_Core_##name


JNI_CORE_FUNCTION(jlong, initCore)(
        JNIEnv* env,
        jobject _,
        jstring appSupportPath,
        jobject networkProvider,
        jobject securityProvider,
        jobject notifier,
        jobject platformUtils) {

    const char* path_chars = env->GetStringUTFChars(appSupportPath, nullptr);
    std::string cpp_app_support_path(path_chars);
    env->ReleaseStringUTFChars(appSupportPath, path_chars);

    jobject globalNetwork = env->NewGlobalRef(networkProvider);
    jobject globalSecurity = env->NewGlobalRef(securityProvider);
    jobject globalNotifier = env->NewGlobalRef(notifier);
    jobject globalPlatformUtils = env->NewGlobalRef(platformUtils);

    auto* cpp_network = new JavaNetworkProvider(env, globalNetwork);
    auto* cpp_security = new JavaSecureStorageProvider(env, globalSecurity);
    auto* cpp_notifier = new JavaNotifier(env, globalNotifier);
    auto* cpp_platform_utils = new JavaPlatformUtils(env, globalPlatformUtils);

    Core* core = new Core(
            cpp_app_support_path,
            *cpp_network,
            *cpp_security,
            *cpp_notifier,
            *cpp_platform_utils

    );
    return reinterpret_cast<jlong>(core);
}

JNI_CORE_FUNCTION(void, initialize)(COMMON_PARAMS) {
    CORE(ptr)
    core->initialize();
}

JNI_CORE_FUNCTION(void, destroy)(COMMON_PARAMS) {
    CORE(ptr)
    core->destroy();
}

JNI_CORE_FUNCTION(void, onSuspend)(COMMON_PARAMS) {
    CORE(ptr)
    core->on_suspend();
}

JNI_CORE_FUNCTION(void, onResume)(COMMON_PARAMS) {
    CORE(ptr)
    core->on_resume();
}

JNI_CORE_FUNCTION(void, addUser)(COMMON_PARAMS) {
    CORE(ptr)
    core->add_user();
}

JNI_CORE_FUNCTION(void, switchUser)(COMMON_PARAMS, jchar userLocalId) {
    CORE(ptr)
    core->switch_user(static_cast<char>(userLocalId));
}

JNI_CORE_FUNCTION(void, handleLoginOnFirstRun)(COMMON_PARAMS, jstring jInstanceUrl, jstring jEmail, jstring jPassword) {
    CORE(ptr)
    std::string instanceUrl = jstring_to_string(env, jInstanceUrl);
    std::string email = jstring_to_string(env, jEmail);
    std::string password = jstring_to_string(env, jPassword);
    core->handle_login(instanceUrl, email, password);
}

JNI_CORE_FUNCTION(void, handleLogin)(COMMON_PARAMS, jstring jEmail, jstring jPassword) {
    CORE(ptr)
    std::string email = jstring_to_string(env, jEmail);
    std::string password = jstring_to_string(env, jPassword);
    core->handle_login(email, password);
}

JNI_CORE_FUNCTION(void, handleRegisterOnFirstRun)(COMMON_PARAMS, jstring jInstanceUrl, jstring jEmail, jstring jPassword, jstring jName) {
    CORE(ptr)
    std::string instanceUrl = jstring_to_string(env, jInstanceUrl);
    std::string email = jstring_to_string(env, jEmail);
    std::string password = jstring_to_string(env, jPassword);
    std::string name = jstring_to_string(env, jName);
    core->handle_register(instanceUrl, email, password, name);
}

JNI_CORE_FUNCTION(void, handleRegister)(COMMON_PARAMS, jstring jEmail, jstring jPassword, jstring jName) {
    CORE(ptr)
    std::string email = jstring_to_string(env, jEmail);
    std::string name = jstring_to_string(env, jName);
    std::string password = jstring_to_string(env, jPassword);
    core->handle_register(email, password, name);
}

JNI_CORE_FUNCTION(void, handleLogout)(COMMON_PARAMS) {
    CORE(ptr)
    core->handle_logout();
}

JNI_CORE_FUNCTION(jobjectArray, getUsers)(COMMON_PARAMS) {
    CORE(ptr)
    jclass userClass = env->FindClass("com/amqhi/cloud/core/User");
    jmethodID userInit = env->GetMethodID(userClass, "<init>", "(CLjava/lang/String;Ljava/lang/String;SLjava/lang/String;)V");
    auto array_size = static_cast<jsize>(core->users().size());
    jobjectArray array = env->NewObjectArray(array_size, userClass, nullptr);
    for (int i = 0; i < core->users().size(); i++)
    {
        const User& user = core->users()[i];
        env->SetObjectArrayElement(array, i, user_to_juser(env, userClass, userInit, user));
    }

    return array;
}

JNI_CORE_FUNCTION(jobjectArray, getUnselectedUsers)(COMMON_PARAMS) {
    CORE(ptr)
    jclass userClass = env->FindClass("com/amqhi/cloud/core/User");
    jmethodID userInit = env->GetMethodID(userClass, "<init>", "(CLjava/lang/String;Ljava/lang/String;SLjava/lang/String;)V");
    auto array_size = static_cast<jsize>(core->users().size() - 1);
    jobjectArray array = env->NewObjectArray(array_size, userClass, nullptr);
    int j = 0;
    for (int i = 0; i < core->users().size(); i++)
    {
        const User& user = core->users()[i];
        if(core->selected_user()->local_id != user.local_id) {
            env->SetObjectArrayElement(array, j++, user_to_juser(env, userClass, userInit, user));
        }
    }
    env->DeleteLocalRef(userClass);
    return array;
}

JNI_CORE_FUNCTION(jobject, getSelectedUser)(COMMON_PARAMS) {
    CORE(ptr)
    jclass userClass = env->FindClass("com/amqhi/cloud/core/User");
    jmethodID userInit = env->GetMethodID(userClass, "<init>", "(CLjava/lang/String;Ljava/lang/String;SLjava/lang/String;)V");
    jobject jUser = user_to_juser(env, userClass, userInit, *core->selected_user());
    env->DeleteLocalRef(userClass);
    return jUser;
}
