#ifndef SHARED_JAVA_NOTIFIER_H
#define SHARED_JAVA_NOTIFIER_H

#include <jni.h>

#include "java_string.h"
#include "notifier.h"

class JavaNotifier : public INotifier {
private:
    JavaVM* jvm_{};
    jobject java_provider_global_;

    [[nodiscard]] JNIEnv* getEnv() const {
        JNIEnv* env;
        if (jvm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
            jvm_->AttachCurrentThread(&env, nullptr);
        }
        return env;
    }

public:
    JavaNotifier(JNIEnv* env, jobject java_provider) {
        env->GetJavaVM(&jvm_);
        java_provider_global_ = env->NewGlobalRef(java_provider);
    }

    ~JavaNotifier() override {
        JNIEnv* env = getEnv();
        if (env && java_provider_global_) {
            env->DeleteGlobalRef(java_provider_global_);
        }
    }
    void notify(int16_t code) override {
        JNIEnv* env = getEnv();
        if (!env || !java_provider_global_) return;

        jclass clazz = env->GetObjectClass(java_provider_global_);
        jmethodID methodId = env->GetMethodID(clazz, "notify", "(S)V");

        if (methodId != nullptr) {
            env->CallVoidMethod(java_provider_global_, methodId, static_cast<jshort>(code));
        }
        env->DeleteLocalRef(clazz);
    }

    void notify(int16_t code, const std::string &data) override {
        JNIEnv* env = getEnv();
        if (!env || !java_provider_global_) return;

        jclass clazz = env->GetObjectClass(java_provider_global_);
        jmethodID methodId = env->GetMethodID(clazz, "notify", "(SLjava/lang/String;)V");

        if (methodId != nullptr) {
            JString jData(env, data);
            env->CallVoidMethod(java_provider_global_, methodId, static_cast<jshort>(code), jData.value);
        }
        env->DeleteLocalRef(clazz);
    }

    void notify(int16_t code, const nlohmann::json& data) override {
        JNIEnv* env = getEnv();
        if (!env || !java_provider_global_) return;

        jclass clazz = env->GetObjectClass(java_provider_global_);
        jmethodID methodId = env->GetMethodID(clazz, "notifyJson", "(SLjava/lang/String;)V");

        if (methodId != nullptr) {
            JString jData(env, data);
            env->CallVoidMethod(java_provider_global_, methodId, static_cast<jshort>(code), jData.value);
        }
        env->DeleteLocalRef(clazz);
    }
};

#endif //SHARED_JAVA_NOTIFIER_H
