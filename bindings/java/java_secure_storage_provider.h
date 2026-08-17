#ifndef SHARED_JAVA_SECURE_STORAGE_PROVIDER_H
#define SHARED_JAVA_SECURE_STORAGE_PROVIDER_H

#include <jni.h>

#include "secure_storage_provider.h"

class JavaSecureStorageProvider : public ISecureStorageProvider {
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
    JavaSecureStorageProvider(JNIEnv* env, jobject java_provider) {
        env->GetJavaVM(&jvm_);
        java_provider_global_ = env->NewGlobalRef(java_provider);
    }

    ~JavaSecureStorageProvider() override {
        JNIEnv* env = getEnv();
        if (env && java_provider_global_) {
            env->DeleteGlobalRef(java_provider_global_);
        }
    }

    bool set_secure_string(const std::string& key, const std::string& value) override {
        JNIEnv* env = getEnv();
        jclass clazz = env->GetObjectClass(java_provider_global_);
        jmethodID methodId = env->GetMethodID(clazz, "setSecureString", "(Ljava/lang/String;Ljava/lang/String;)Z");

        jstring jkey = env->NewStringUTF(key.c_str());
        jstring jval = env->NewStringUTF(value.c_str());

        jboolean result = env->CallBooleanMethod(java_provider_global_, methodId, jkey, jval);

        env->DeleteLocalRef(jkey);
        env->DeleteLocalRef(jval);
        return result == JNI_TRUE;
    }

    std::string get_secure_string(const std::string& key, const std::string& default_value) override {
        JNIEnv* env = getEnv();
        jclass clazz = env->GetObjectClass(java_provider_global_);
        jmethodID methodId = env->GetMethodID(clazz, "getSecureString", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");

        jstring jkey = env->NewStringUTF(key.c_str());
        jstring jdefault = env->NewStringUTF(default_value.c_str());

        auto jres = reinterpret_cast<jstring>(env->CallObjectMethod(java_provider_global_, methodId, jkey, jdefault));

        const char* res_chars = env->GetStringUTFChars(jres, nullptr);
        std::string result(res_chars);
        env->ReleaseStringUTFChars(jres, res_chars);

        env->DeleteLocalRef(jkey);
        env->DeleteLocalRef(jdefault);
        env->DeleteLocalRef(jres);
        return result;
    }

    void remove_secure_string(const std::string& key) override {
        JNIEnv* env = getEnv();
        jclass clazz = env->GetObjectClass(java_provider_global_);
        jmethodID methodId = env->GetMethodID(clazz, "removeSecureString", "(Ljava/lang/String;)V");

        jstring jkey = env->NewStringUTF(key.c_str());
        env->CallVoidMethod(java_provider_global_, methodId, jkey);
        env->DeleteLocalRef(jkey);
    }
};

#endif //SHARED_JAVA_SECURE_STORAGE_PROVIDER_H
