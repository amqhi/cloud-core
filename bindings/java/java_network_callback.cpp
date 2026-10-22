#include <jni.h>

#include "core.h"
#include "java_map.h"
#include "java_string.h"

struct CallbackContext {
    std::function<void(const amqhi::HttpResponse&)> response;
    std::function<void(std::int16_t, const std::string&)> failure;
};

extern "C" JNIEXPORT void JNICALL
Java_com_amqhi_cloud_core_NetworkCallback_onResponse(
        JNIEnv* env,
        jobject,
        jlong ptr,
        jint statusCode,
        jstring jResponse,
        jobject jHeaders) {

    auto context = reinterpret_cast<CallbackContext*>(ptr);

    amqhi::HttpResponse response;
    response.status_code = statusCode;
    response.body = jstring_to_string(env, jResponse);
    response.headers = java_map::jobject_to_unordered_map(env, jHeaders);

    context->response(response);

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
