//
// Created by huangyong on 2019/8/20.
//

#include "LogHelper.h"
#include <jni.h>

/**
 * 备注：这里的ifdef以及代码结束处的ifdef必须加上，否则程序会运行报错
 * 写JNI代码时一定要小心，对JNI的方法都要进行异常检测，
 * 对调用Java可能会抛出异常的代码使用JNI提供的异常处理方法处理。
 */
#ifdef __cplusplus
extern "C" {
#endif

//动态注册的类（需要一个完整的类路径）
static const char *mClassName = "com/hy/ndk/mediastudy/MediaTest";

/**
* 全局JVM变量，方便随后调用
*/
JavaVM *g_jvm = nullptr;
jobject g_jobj = nullptr;

void init(JNIEnv *env, jobject instance) {
    LOGI("native层init初始化");
    g_jobj = env->NewGlobalRef(instance);
}

void destroy(JNIEnv *env, jobject instance) {
    LOGI("native层销毁");
    env->DeleteGlobalRef(g_jobj);
}

//静态的native方法签名数组
static const JNINativeMethod method[] = {
        {"init",    "()V", (void *) init},
        {"destroy", "()V", (void *) destroy}
};

/**
 * 注册本地方法
 * @param env
 * @return
 */
static jint registerNative(JNIEnv *env) {
    jclass cls = env->FindClass(mClassName);
    if (cls == nullptr) {
        return JNI_FALSE;
    }

    //动态注册需要调用该方法
    if (env->RegisterNatives(cls, method, sizeof(method) / sizeof(method[0])) < 0) {
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

/**
 * 加载本地库时调用
 * 这里可以做一些定制操作，比如动态注册
 */
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *jvm, void *reserved) {
    LOGI("Jni_OnLoad");

    g_jvm = jvm;

    JNIEnv *env = nullptr;
    jint res = jvm->GetEnv((void **) &env, JNI_VERSION_1_6);

    if (res != JNI_OK) {
        return JNI_ERR;
    }

    if (!registerNative(env)) {
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

/**
 * 在卸载本地库时调用
 * @param vm
 * @param reserved
 */
JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM *jvm, void *reserved) {
    LOGI("JNI_OnUnload");

    JNIEnv *env;
    if (jvm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return;
    }
}


#ifdef __cplusplus
}
#endif