
#ifndef CLOUD_CORE_JAVA_BINDING_MACROS_H
#define CLOUD_CORE_JAVA_BINDING_MACROS_H

#define CORE(ptr) Core* core = reinterpret_cast<Core*>(ptr);
#define COMMON_PARAMS JNIEnv* env, jobject thiz, jlong ptr

#define UUID_PARAMS jlong jIdHigh, jlong jIdLow
#define UUID_FROM_PARAMS(name) UUID name{}; \
name.high = jIdHigh; \
name.low = jIdLow;

#define JNI_GET_OBJECT_FROM_CLASS(jObj, name, field, sig) jfieldID name##Field = env->GetFieldID(clazz, field, sig); \
        jobject name = env->GetObjectField(jObj, name##Field);

#define JNI_GET_LONG_FROM_CLASS(jObj, name, field) jfieldID name##Field = env->GetFieldID(clazz, field, "J"); \
jlong name = env->GetLongField(jObj, name##Field);

#define JNI_GET_BOOLEAN_FROM_CLASS(jObj, name, field) jfieldID name##Field = env->GetFieldID(clazz, field, "Z"); \
        jboolean name = env->GetBooleanField(jObj, name##Field);

#define JNI_GET_STRING_FROM_CLASS(jObj, name, field) jfieldID name##Field = env->GetFieldID(clazz, field, "Ljava/lang/String;"); \
jstring name = reinterpret_cast<jstring>(env->GetObjectField(jObj, name##Field));

#define JNI_GET_SHORT_FROM_CLASS(jObj, name, field) jfieldID name##Field = env->GetFieldID(clazz, field, "S"); \
jshort name = env->GetShortField(jObj, name##Field);

#define JNI_GET_INT_FROM_CLASS(jObj, name, field) jfieldID name##Field = env->GetFieldID(clazz, field, "I"); \
jint name = env->GetIntField(jObj, name##Field);

#endif //CLOUD_CORE_JAVA_BINDING_MACROS_H
