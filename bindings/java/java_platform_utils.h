#ifndef CLOUD_CORE_ANDROID_PLATFORM_UTILS_H
#define CLOUD_CORE_ANDROID_PLATFORM_UTILS_H
#include "platform_utils.h"
#include "java_string.h"
#include <jni.h>

class JavaPlatformUtils : public IPlatformUtils
{
private:
    JavaVM *jvm_{};
    jobject java_provider_global_;

    [[nodiscard]] JNIEnv *getEnv() const {
        JNIEnv *env;
        if (jvm_->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
            jvm_->AttachCurrentThread(&env, nullptr);
        }
        return env;
    }
public:
    JavaPlatformUtils(JNIEnv *env, jobject java_provider) {
        env->GetJavaVM(&jvm_);
        java_provider_global_ = env->NewGlobalRef(java_provider);
    }

    ~JavaPlatformUtils() override {
        JNIEnv *env = getEnv();
        if (env && java_provider_global_) {
            env->DeleteGlobalRef(java_provider_global_);
        }
    }

    [[nodiscard]] std::string get_file_sha256_checksum(const std::string& file_path) const override
    {
        JNIEnv *env = getEnv();
        jclass clazz = env->GetObjectClass(java_provider_global_);
        jmethodID methodId = env->GetMethodID(clazz, "getFileSha256Checksum",
                                             "(Ljava/lang/String;)Ljava/lang/String;");
        JString jFilePath(env, file_path);

        auto jresult = static_cast<jstring>(env->CallObjectMethod(java_provider_global_, methodId, jFilePath.value));

        std::string result = jstring_to_string(env, jresult);
        env->DeleteLocalRef(jresult);

        return result;
    }
};

#endif //CLOUD_CORE_ANDROID_PLATFORM_UTILS_H
