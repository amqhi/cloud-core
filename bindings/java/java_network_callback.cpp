#include <jni.h>

#include "core.h"

struct CallbackContext {
    std::function<void(int, const std::string&)> response;
    std::function<void(std::int16_t, const std::string&)> failure;
};

extern "C" JNIEXPORT void JNICALL
Java_com_amqhi_cloud_core_NetworkCallback_onResponse(
        JNIEnv* env,
        jobject,
        jlong ptr,
        jint statusCode,
        jstring response) {

    auto context = reinterpret_cast<CallbackContext*>(ptr);

    const char* res_chars = env->GetStringUTFChars(response, nullptr);
    std::string cpp_response(res_chars);
    env->ReleaseStringUTFChars(response, res_chars);

    context->response(statusCode, cpp_response);

    delete context;
}

extern "C" JNIEXPORT void JNICALL
Java_com_amqhi_cloud_core_NetworkCallback_onFailure(
        JNIEnv* env,
        jobject,
        jlong ptr,
        jshort errorCode,
        jstring data) {

    auto context = reinterpret_cast<CallbackContext*>(ptr);

    const char* res_chars = env->GetStringUTFChars(data, nullptr);
    std::string cpp_data(res_chars);
    env->ReleaseStringUTFChars(data, res_chars);

    context->failure(static_cast<std::int16_t>(errorCode), cpp_data);

    delete context;
}
