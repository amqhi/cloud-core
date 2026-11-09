#include <jni.h>

#include "core.h"
#include "java_binding_macros.h"
#include "java_integer.h"
#include "java_item.h"

using namespace amqhi;

#define JNI_ITEM_MANAGER_FUNCTION(type, name) \
extern "C" JNIEXPORT type JNICALL \
Java_com_amqhi_cloud_core_ItemManager_##name

JNI_ITEM_MANAGER_FUNCTION(jobjectArray, getItemSummaryListById)(
    COMMON_PARAMS,
    jlong jFolderIdHigh,
    jlong jFolderIdLow)
{
    CORE(ptr)
    UUID folderId{};
    folderId.high = jFolderIdHigh;
    folderId.low = jFolderIdLow;
    const auto id_list = core->item_manager().id_list_by_id(folderId);
    jclass summaryClass = env->FindClass("com/amqhi/shared/core/ItemSummary");
    auto array_size = static_cast<jsize>(id_list.size());
    jobjectArray array = env->NewObjectArray(array_size, summaryClass, nullptr);
    for (int i = 0; i < id_list.size(); i++)
    {
        const auto& item_id = id_list[i];
        const auto& item = core->item_manager().item_by_id(item_id);
        JItemSummary jSummary(env, summaryClass, item, core->app_support_path(), core->selected_user()->local_id);
        env->SetObjectArrayElement(array, i, jSummary.value);
    }

    env->DeleteLocalRef(summaryClass);

    return array;
}

JNI_ITEM_MANAGER_FUNCTION(void, createFolder)(COMMON_PARAMS,
                                              jobject j_item_attributes)
{
    CORE(ptr)
    ItemAttributes attributes = item_attributes_from_jobject(env, j_item_attributes);
    core->item_manager().create_folder(attributes);
}

JNI_ITEM_MANAGER_FUNCTION(void, fetchFileDownloadUrl)(
    COMMON_PARAMS,
    jlong jIdHigh,
    jlong jIdLow)
{
    CORE(ptr)
    UUID id{};
    id.high = jIdHigh;
    id.low = jIdLow;
    core->item_manager().fetch_file_download_url(id);
}

JNI_ITEM_MANAGER_FUNCTION(jobject, getItemSummaryById)(
    COMMON_PARAMS,
    jlong jIdHigh,
    jlong jIdLow)
{
    CORE(ptr)
    UUID id{};
    id.high = jIdHigh;
    id.low = jIdLow;
    const Item& item = core->item_manager().item_by_id(id);
    JItemSummary jItemSummary(env, item, core->app_support_path(), core->selected_user()->local_id);

    return jItemSummary.value;
}

JNI_ITEM_MANAGER_FUNCTION(void, renameItem)(COMMON_PARAMS, jlong idHigh, jlong idLow, jstring jItemName)
{
    CORE(ptr)
    UUID id{};
    id.high = idHigh;
    id.low = idLow;
    std::string name = jstring_to_string(env, jItemName);
    core->item_manager().rename_item(id, name);
}

JNI_ITEM_MANAGER_FUNCTION(void, updateItem)(COMMON_PARAMS, SIMPLE_UUID_PARAMS, jobject jItemAttributes)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    ItemAttributes attributes = item_attributes_from_jobject(env, jItemAttributes);
    core->item_manager().update_item(id, attributes);
}

JNI_ITEM_MANAGER_FUNCTION(void, refresh)(COMMON_PARAMS)
{
    CORE(ptr)
    core->item_manager().refresh();
}

JNI_ITEM_MANAGER_FUNCTION(void, createFile)(COMMON_PARAMS,
                                            jobject j_item_attributes,
                                            jstring j_tmp_file_path)
{
    Core* core = reinterpret_cast<Core*>(ptr);
    ItemAttributes cpp_attributes = item_attributes_from_jobject(env, j_item_attributes);

    std::string tmp_file_path = jstring_to_string(env, j_tmp_file_path);

    core->item_manager().create_file(cpp_attributes, tmp_file_path);
}

JNI_ITEM_MANAGER_FUNCTION(jstring, getItemOfflineFilePathById)(
    COMMON_PARAMS,
    SIMPLE_UUID_PARAMS
    )
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    jstring jFilePath = env->NewStringUTF(core->item_manager().item_offline_file_path_by_id(id).c_str());
    return jFilePath;
}

JNI_ITEM_MANAGER_FUNCTION(jobject, getItemById)(
        COMMON_PARAMS,
        SIMPLE_UUID_PARAMS
)
 {
    CORE(ptr)
     SIMPLE_UUID_FROM_PARAMS(id)
    jobject jItem = item_to_jobject(env, core->item_manager().item_by_id(id));
    return jItem;
}

JNI_ITEM_MANAGER_FUNCTION(jobject, getFileMetadataById)(COMMON_PARAMS, SIMPLE_UUID_PARAMS) {
CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    const FileMetadata& file_metadata = core->item_manager().file_metadata_by_id(id);
    jclass clazz = env->FindClass("com/amqhi/shared/core/FileMetadata");
    jmethodID methodId = env->GetMethodID(clazz, "<init>",
                                            "(Ljava/lang/String;Ljava/lang/String;J)V");
    JString checksum(env, file_metadata.checksum);
    JString mimeType(env, file_metadata.mime_type);
    auto size = static_cast<jlong>(file_metadata.size);
    jobject metadata = env->NewObject(clazz, methodId,
                                      checksum.value,
                                      mimeType.value,
                                      size
    );
    return metadata;
}

JNI_ITEM_MANAGER_FUNCTION(void, downloadItem)(COMMON_PARAMS, SIMPLE_UUID_PARAMS, jstring jFilePath)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    std::string filePath = jstring_to_string(env, jFilePath);
    core->item_manager().download_item(id, filePath);
}

JNI_ITEM_MANAGER_FUNCTION(jobject, getFolderMetadataById)(COMMON_PARAMS, SIMPLE_UUID_PARAMS) {
    CORE(ptr)
        SIMPLE_UUID_FROM_PARAMS(id)
    const FolderMetadata& folder_metadata = core->item_manager().folder_metadata_by_id(id);
        jclass clazz = env->FindClass("com/amqhi/shared/core/FolderMetadata");
    jmethodID methodId = env->GetMethodID(clazz, "<init>",
                                            "(Ljava/util/UUID;Ljava/util/UUID;Ljava/lang/Integer;Ljava/lang/Integer;)V");
    JUUID backgroundId(env, folder_metadata.background_id.high, folder_metadata.background_id.low);
    JUUID iconId(env, folder_metadata.icon_id.high, folder_metadata.icon_id.low);
    jobject backgroundColor = nullptr;
    jobject iconColor = nullptr;
    jclass intClazz = env->FindClass("java/lang/Integer");
    jmethodID intMethodId = env->GetMethodID(intClazz, "<init>", "(I)V");
    if (folder_metadata.background_color.has_value())
    {
        backgroundColor = env->NewObject(intClazz, intMethodId, folder_metadata.background_color.value());
    }
    if (folder_metadata.icon_color.has_value())
    {
        iconColor = env->NewObject(intClazz, intMethodId, folder_metadata.icon_color.value());
    }
    jobject metadata = env->NewObject(clazz, methodId,
                                      backgroundId.value,
                                      iconId.value,
                                      backgroundColor,
                                      iconColor
    );

    if (backgroundColor != nullptr)
    {
        env->DeleteLocalRef(backgroundColor);
    }
    if (iconColor != nullptr)
    {
        env->DeleteLocalRef(iconColor);
    }
    return metadata;
}

JNI_ITEM_MANAGER_FUNCTION(jchar, getFolderViewModeById)(
    COMMON_PARAMS, SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    return static_cast<jchar>(core->item_manager().folder_view_mode_by_id(id));
}

JNI_ITEM_MANAGER_FUNCTION(jchar, getFolderSortOptionById)(
    COMMON_PARAMS, SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    return static_cast<jchar>(core->item_manager().folder_sort_option_by_id(id));
}

JNI_ITEM_MANAGER_FUNCTION(void, downloadThumbnail)(
    COMMON_PARAMS,
    SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id);
    core->item_manager().download_thumbnail(id);
}

JNI_ITEM_MANAGER_FUNCTION(void, sortItems)(
        COMMON_PARAMS,
        jchar jOption,
        SIMPLE_UUID_PARAMS
        ) {
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(folder_id);
    core->item_manager().sort_items(static_cast<char>(jOption), folder_id);
}

JNI_ITEM_MANAGER_FUNCTION(void, moveItem)(
    COMMON_PARAMS,
    jlong jIdHigh,
    jlong jIdLow,
    jlong jParentIdHigh,
    jlong jParentIdLow)
{
    CORE(ptr)
    UUID id{};
    id.high = jIdHigh;
    id.low = jIdLow;
    UUID parentId{};
    parentId.high = jParentIdHigh;
    parentId.low = jParentIdLow;
    core->item_manager().move_item(id, parentId);
}

JNI_ITEM_MANAGER_FUNCTION(void, moveItems)(
COMMON_PARAMS,
jlongArray jIdValues,
UUID_PARAMS(parentId),
UUID_PARAMS(oldParentId))
{
    CORE(ptr)
    std::vector<UUID> ids = java_uuid::vector_from_jlongArray(env, jIdValues);
    UUID_FROM_PARAMS(parentId)
    UUID_FROM_PARAMS(oldParentId)

    core->item_manager().move_items(ids, parentId, oldParentId);
}

JNI_ITEM_MANAGER_FUNCTION(void, softDeleteItem)(COMMON_PARAMS, SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    core->item_manager().soft_delete_item(id);
}

JNI_ITEM_MANAGER_FUNCTION(void, softDeleteItems)(
COMMON_PARAMS,
jlongArray jIdValues,
UUID_PARAMS(oldParentId))
{
    CORE(ptr)
    std::vector<UUID> ids = java_uuid::vector_from_jlongArray(env, jIdValues);
    UUID_FROM_PARAMS(oldParentId)

    core->item_manager().soft_delete_items(ids, oldParentId);
}

JNI_ITEM_MANAGER_FUNCTION(void, restoreItem)(COMMON_PARAMS, SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    core->item_manager().restore_item(id);
}

JNI_ITEM_MANAGER_FUNCTION(void, restoreItems)(
COMMON_PARAMS,
jlongArray jIdValues,
UUID_PARAMS(oldParentId))
{
    CORE(ptr)
    std::vector<UUID> ids = java_uuid::vector_from_jlongArray(env, jIdValues);
    UUID_FROM_PARAMS(oldParentId)

    core->item_manager().restore_items(ids, oldParentId);
}

JNI_ITEM_MANAGER_FUNCTION(void, deleteItem)(COMMON_PARAMS, SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    core->item_manager().delete_item(id);
}

JNI_ITEM_MANAGER_FUNCTION(void, deleteItems)(
COMMON_PARAMS,
jlongArray jIdValues)
{
    CORE(ptr)
    std::vector<UUID> ids = java_uuid::vector_from_jlongArray(env, jIdValues);

    core->item_manager().delete_items(ids);
}

JNI_ITEM_MANAGER_FUNCTION(void, makeItemAvailableOffline)(COMMON_PARAMS, SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    core->item_manager().make_item_available_offline(id);
}

JNI_ITEM_MANAGER_FUNCTION(void, makeItemOnlineOnly)(COMMON_PARAMS, SIMPLE_UUID_PARAMS)
{
    CORE(ptr)
    SIMPLE_UUID_FROM_PARAMS(id)
    core->item_manager().make_item_online_only(id);
}