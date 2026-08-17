#include <jni.h>

#include "core.h"
#include "java_network_provider.h"
#include "java_secure_storage_provider.h"
#include "java_notifier.h"
#include "java_platform_utils.h"
#include "item_utils.h"
#include "java_item.h"

#define CORE(ptr) Core* core = reinterpret_cast<Core*>(ptr)
// TODO: Allow configurable package path via arguments
#define JNI_FUNCTION(type, name) \
extern "C" JNIEXPORT type JNICALL \
Java_com_amqhi_cloud_core_Core_##name

// TODO: Allow configurable package path via arguments
#define JNI_PREFIX(name) Java_com_amqhi_cloud_core_Core_ ## name
#define COMMON_PARAMS JNIEnv* env, jobject thiz, jlong ptr

struct CallbackContext {
    std::function<void(int, const std::string&)> response;
    std::function<void(int, const std::string&)> failure;
};

struct ObjectStorageCallbackContext {
    std::function<void(std::int64_t, std::int64_t)> progress;
    std::function<void(int, const std::string &)> response;
    std::function<void(int, const std::string &)> failure;
};

JNI_FUNCTION(jlong, initCore)(
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

JNI_FUNCTION(void, initialize)(COMMON_PARAMS) {
    CORE(ptr);
    core->initialize();
}

extern "C" JNIEXPORT jobjectArray JNICALL
JNI_PREFIX(getUsers)(JNIEnv *env, jobject thiz, jlong ptr) {
    Core* core = reinterpret_cast<Core*>(ptr);
    jclass item_class = env->FindClass("com/amqhi/shared/model/User");
    auto array_size = static_cast<jsize>(core->users().size());
    jobjectArray array = env->NewObjectArray(array_size, item_class, nullptr);
    for (int i = 0; i < core->users().size(); i++)
    {
        const User& user = core->users()[i];
        jmethodID userInit = env->GetMethodID(item_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;SLjava/lang/String;)V");
        jstring j_local_id = env->NewStringUTF(user.local_id.c_str());
        jstring j_user_name = env->NewStringUTF(user.name.c_str());
        jstring j_email = env->NewStringUTF(user.email.c_str());
        jstring j_cached_picture_path = nullptr;
        if (!user.cached_picture_path.empty()) {
            j_cached_picture_path = env->NewStringUTF(user.cached_picture_path.c_str());
        }
        auto j_login_type = static_cast<jshort>(user.login_type);
        jobject jitem = env->NewObject(item_class, userInit, j_local_id, j_user_name, j_email, j_login_type, j_cached_picture_path);
        env->SetObjectArrayElement(array, i, jitem);
        env->DeleteLocalRef(jitem);
        env->DeleteLocalRef(j_local_id);
        env->DeleteLocalRef(j_user_name);
        env->DeleteLocalRef(j_email);
        if (j_cached_picture_path != nullptr) {
            env->DeleteLocalRef(j_cached_picture_path);
        }
    }

    return array;
}

extern "C" JNIEXPORT void JNICALL
Java_com_amqhi_cloud_core_NativeNetworkCallbackProxy_onResponse(
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
Java_com_amqhi_cloud_core_NativeNetworkCallbackProxy_onFailure(
        JNIEnv* env,
        jobject,
        jlong ptr,
        jint errorCode,
        jstring data) {

    auto context = reinterpret_cast<CallbackContext*>(ptr);

    const char* res_chars = env->GetStringUTFChars(data, nullptr);
    std::string cpp_data(res_chars);
    env->ReleaseStringUTFChars(data, res_chars);

    context->failure(errorCode, cpp_data);

    delete context;
}

extern "C" JNIEXPORT void JNICALL
Java_com_amqhi_cloud_core_ObjectStorageCallbackProxy_onProgress(
        JNIEnv* env,
        jobject,
        jlong ptr,
        jlong bytes_transferred,
        jlong total_bytes) {

    auto context = reinterpret_cast<ObjectStorageCallbackContext*>(ptr);

    context->progress(bytes_transferred, total_bytes);

    delete context;
}

extern "C" JNIEXPORT void JNICALL
Java_com_amqhi_cloud_core_ObjectStorageCallbackProxy_onResponse(
        JNIEnv* env,
        jobject,
        jlong ptr,
        jint statusCode,
        jstring response) {

    auto context = reinterpret_cast<ObjectStorageCallbackContext*>(ptr);

    const char* res_chars = env->GetStringUTFChars(response, nullptr);
    std::string cpp_response(res_chars);
    env->ReleaseStringUTFChars(response, res_chars);

    context->response(statusCode, cpp_response);

    delete context;
}

extern "C" JNIEXPORT void JNICALL
Java_com_amqhi_cloud_core_ObjectStorageCallbackProxy_onFailure(
        JNIEnv* env,
        jobject,
        jlong ptr,
        jint errorCode,
        jstring data) {

    auto context = reinterpret_cast<ObjectStorageCallbackContext*>(ptr);

    const char* res_chars = env->GetStringUTFChars(data, nullptr);
    std::string cpp_data(res_chars);
    env->ReleaseStringUTFChars(data, res_chars);

    context->failure(errorCode, cpp_data);

    delete context;
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX(exchangeGoogleToken)(
        JNIEnv* env,
        jobject thiz,
        jlong ptr,
        jstring token) {

    Core* core = reinterpret_cast<Core*>(ptr);

    const char* id_token_chars = env->GetStringUTFChars(token, nullptr);
    std::string id_token(id_token_chars);
    env->ReleaseStringUTFChars(token, id_token_chars);
    core->exchange_google_token(id_token);
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX(handleLogin)(
        JNIEnv* env,
        jobject thiz,
        jlong ptr,
        jstring jEmail,
        jstring jPassword) {

    Core* core = reinterpret_cast<Core*>(ptr);

    std::string email = jstring_to_string(env, jEmail);
    std::string password = jstring_to_string(env, jPassword);
    core->handle_login(email, password);
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX(handleRegister)(
        JNIEnv* env,
        jobject _,
        jlong ptr,
        jstring email,
        jstring name,
        jstring password) {

    Core* core = reinterpret_cast<Core*>(ptr);

    const char* email_chars = env->GetStringUTFChars(email, nullptr);
    std::string email_(email_chars);
    env->ReleaseStringUTFChars(email, email_chars);

    const char* password_chars = env->GetStringUTFChars(password, nullptr);
    std::string password_(password_chars);
    env->ReleaseStringUTFChars(password, password_chars);

    const char* name_chars = env->GetStringUTFChars(name, nullptr);
    std::string name_(name_chars);
    env->ReleaseStringUTFChars(name, name_chars);
    core->handle_register(email_, password_, name_);
}

ItemAttributes item_attributes_from_jobject(JNIEnv* env, jobject j_item_attributes)
{
    ItemAttributes cpp_attributes;
      if (j_item_attributes != nullptr) {
        jclass attrClass = env->GetObjectClass(j_item_attributes);

        jfieldID nameField = env->GetFieldID(attrClass, "name", "Ljava/lang/String;");
        auto jName = reinterpret_cast<jstring>(env->GetObjectField(j_item_attributes, nameField));
        cpp_attributes.name = jstring_to_string(env, jName);
        env->DeleteLocalRef(jName);

        jfieldID parentIdField = env->GetFieldID(attrClass, "parentId", "Ljava/lang/String;");
        auto jParentId = reinterpret_cast<jstring>(env->GetObjectField(j_item_attributes, parentIdField));
        cpp_attributes.parent_id = jstring_to_string(env, jParentId);
        env->DeleteLocalRef(jParentId);

          // TODO: parse remaining fields

    }
    return cpp_attributes;
}

extern "C"
JNIEXPORT void JNICALL
JNI_PREFIX(createFile)(JNIEnv *env, jobject _,
                                              jlong ptr,
                                              jobject j_item_attributes,
                                              jstring j_tmp_file_path) {
    Core* core = reinterpret_cast<Core*>(ptr);
    ItemAttributes cpp_attributes = item_attributes_from_jobject(env, j_item_attributes);

    std::string tmp_file_path = jstring_to_string(env, j_tmp_file_path);

    core->item_manager().create_file(cpp_attributes, tmp_file_path);
}

extern "C"
JNIEXPORT void JNICALL
JNI_PREFIX(createFolder)(JNIEnv *env, jobject _,
                                                                                     jlong ptr,
                                                                                     jobject j_item_attributes) {
    Core* core = reinterpret_cast<Core*>(ptr);
    ItemAttributes cpp_attributes = item_attributes_from_jobject(env, j_item_attributes);

    core->item_manager().create_folder(cpp_attributes);
}

extern "C" JNIEXPORT jstring JNICALL
JNI_PREFIX(getInstanceUrl)(
        JNIEnv* env,
        jobject _,
        jlong ptr) {

    Core* core = reinterpret_cast<Core*>(ptr);

    std::string instance_url = core->settings().data().instance_url;

    return env->NewStringUTF(instance_url.c_str());
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX(setInstanceUrl)(
        JNIEnv* env,
        jobject _,
        jlong ptr,
        jstring instanceUrl) {

    CORE(ptr);

    const char* cString = env->GetStringUTFChars(instanceUrl, nullptr);
    std::string instance_url(cString);

    core->settings().set_instance_url(instance_url);
    env->ReleaseStringUTFChars(instanceUrl, cString);
    // const char* cString = env->GetStringUTFChars(instanceUrl, nullptr);
    // Core_setInstanceUrl(&ptr, cString);
    // env->ReleaseStringUTFChars(instanceUrl, cString);
}

extern "C" JNIEXPORT jobjectArray JNICALL
JNI_PREFIX(getItemSummaryListById)(
        JNIEnv* env,
        jobject _,
        jlong ptr,
        jstring jid) {

    Core* core = reinterpret_cast<Core*>(ptr);

    std::string id = jstring_to_string(env, jid);
    const std::vector<ItemSummary>& summary_list = core->item_manager().summary_list_by_id(id);
    jclass summaryClass = env->FindClass("com/amqhi/cloud/models/ItemSummary");
    auto array_size = static_cast<jsize>(summary_list.size());
    jobjectArray array = env->NewObjectArray(array_size, summaryClass, nullptr);
    for (int i = 0; i < summary_list.size(); i++)
    {
        const ItemSummary& summary = summary_list[i];
        JItemSummary jSummary(env, summaryClass, summary);
        env->SetObjectArrayElement(array, i, jSummary.value);
    }

    env->DeleteLocalRef(summaryClass);

    return array;
}

extern "C" JNIEXPORT jobject JNICALL
JNI_PREFIX(getItemSummaryById)(
        JNIEnv* env,
        jobject _,
        jlong ptr,
        jstring jid) {
    Core* core = reinterpret_cast<Core*>(ptr);
    std::string id = jstring_to_string(env, jid);
    const Item& item = core->item_manager().item_by_id(id);

    jclass item_class = env->FindClass("com/amqhi/cloud/models/ItemSummary");
    jmethodID cbInit = env->GetMethodID(item_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jstring jitem_id = env->NewStringUTF(item.id.c_str());
    jstring jitem_type = env->NewStringUTF(item.type.c_str());
    jstring jitem_name = env->NewStringUTF(item.name.c_str());
    jstring jitem_thumbnail_path = env->NewStringUTF(item_thumbnail_path(*core, id).c_str());
    jobject jitem = env->NewObject(item_class, cbInit, jitem_id, jitem_type, jitem_name, jitem_thumbnail_path);
    env->DeleteLocalRef(jitem);
    env->DeleteLocalRef(jitem_id);
    env->DeleteLocalRef(jitem_type);
    env->DeleteLocalRef(jitem_name);
    env->DeleteLocalRef(jitem_thumbnail_path);

    return jitem;
}


extern "C" JNIEXPORT jobject JNICALL
JNI_PREFIX(getItemById)(
        JNIEnv* env,
        jobject _,
        jlong ptr,
        jstring jid) {
    Core* core = reinterpret_cast<Core*>(ptr);
    std::string id = jstring_to_string(env, jid);
    const Item& item = core->item_manager().item_by_id(id);

    jclass item_class = env->FindClass("com/amqhi/cloud/models/Item");
    jmethodID constructor = env->GetMethodID(item_class, "<init>",
                                             "(Ljava/lang/String;Ljava/lang/String;JJLjava/lang/Long;Ljava/lang/Long;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZI)V");

    if (constructor == nullptr) return nullptr;

    jclass long_class = env->FindClass("java/lang/Long");
    jmethodID long_init = env->GetMethodID(long_class, "<init>", "(J)V");

    jstring j_id = env->NewStringUTF(item.id.c_str());
    jstring j_type = env->NewStringUTF(item.type.c_str());
    jstring j_parent_id = env->NewStringUTF(item.parent_id.c_str());
    jstring j_name = env->NewStringUTF(item.name.c_str());
    // jstring j_tags = env->NewStringUTF(item.tags.c_str());
    // jstring j_comment = env->NewStringUTF(item.comment.c_str());
    jstring j_tags = env->NewStringUTF("");
    jstring j_comment = env->NewStringUTF("");

    jobject j_event_at = nullptr;
    if (item.event_at.has_value()) {
        j_event_at = env->NewObject(long_class, long_init, static_cast<jlong>(item.event_at.value()));
    }

    jobject j_deleted_at = nullptr;
    if (item.deleted_at.has_value()) {
        j_deleted_at = env->NewObject(long_class, long_init, static_cast<jlong>(item.deleted_at.value()));
    }

    jobject item_obj = env->NewObject(item_class, constructor,
                                      j_id,
                                      j_type,
                                      static_cast<jlong>(item.created_at),
                                      static_cast<jlong>(item.updated_at),
                                      j_event_at,
                                      j_deleted_at,
                                      j_parent_id,
                                      j_name,
                                      j_tags,
                                      j_comment,
                                      static_cast<jboolean>(item.encrypted),
                                      static_cast<jint>(item.app_scope)
    );

    env->DeleteLocalRef(j_id);
    env->DeleteLocalRef(j_type);
    env->DeleteLocalRef(j_parent_id);
    env->DeleteLocalRef(j_name);
    env->DeleteLocalRef(j_tags);
    env->DeleteLocalRef(j_comment);

    if (j_event_at != nullptr) {
        env->DeleteLocalRef(j_event_at);
    }
    if (j_deleted_at != nullptr) {
        env->DeleteLocalRef(j_deleted_at);
    }

    return item_obj;
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX(downloadThumbnail)(
        JNIEnv* env,
        jobject _,
        jlong ptr,
        jstring jid) {
    Core* core = reinterpret_cast<Core*>(ptr);
    std::string id = jstring_to_string(env, jid);
    core->item_manager().download_thumbnail(id);
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX(fetchItems)(
        JNIEnv* env,
        jobject _,
        jlong ptr,
        jstring jParentId) {
    Core* core = reinterpret_cast<Core*>(ptr);
    std::string parentId = jstring_to_string(env, jParentId);
    core->item_manager().fetch_items(parentId);
}

extern "C" JNIEXPORT void JNICALL
JNI_PREFIX(fetchFileDownloadUrl)(
    JNIEnv* env,
    jobject _,
    jlong ptr,
    jstring jId) {
        CORE(ptr);
        std::string id = jstring_to_string(env, jId);
        core->item_manager().fetch_file_download_url(id);
    }

JNI_FUNCTION(void, updateItem)(COMMON_PARAMS, jstring jId, jobject jItemAttributes) {
    CORE(ptr);
    ItemAttributes attributes = item_attributes_from_jobject(env, jItemAttributes);
    string id = jstring_to_string(env, jId);
    core->item_manager().update_item(id, attributes);
}

JNI_FUNCTION(void, renameItem)(COMMON_PARAMS, jstring jId, jstring jItemName) {
    CORE(ptr);
    ItemAttributes attributes;
    string id = jstring_to_string(env, jId);
    string name = jstring_to_string(env, jItemName);
    core->item_manager().rename_item(id, name);
}

JNI_FUNCTION(void, openFolder)(COMMON_PARAMS, jstring jId) {
    CORE(ptr);
    string folderId = jstring_to_string(env, jId);
    core->open_folder(folderId);
}