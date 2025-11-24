//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmConfig.hpp"
#include "LlmImpl.hpp"
#include "Logger.hpp"
#include <iostream>
#include <jni.h>
#include "LlmBridge.hpp"


static std::unique_ptr<LLM> llm;

/**
* @brief inline method to throw error in java
* @param env JNI environment variable passed from JVM layer
* @param message error message to be propagated to java/kotlin layer
*/
inline void ThrowJavaException(JNIEnv* env, const char* message) {
    jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
    if (exceptionClass) {
        env->ThrowNew(exceptionClass, message);
    }
}
/**
* @brief  Lambda function to realize RAII utf-strings
* @param env JNI environment variable passed from JVM layer
* @param jStr java string variable to be converted to c-string
* @return reference to converted c-string
*/
auto GetUtfChars = [](JNIEnv* env, jstring jStr) {
    using Deleter = std::function<void(const char*)>;

    const char* chars = env->GetStringUTFChars(jStr, nullptr);
    Deleter deleter = [env, jStr](const char* p) {
        env->ReleaseStringUTFChars(jStr, p);
    };

    return std::unique_ptr<const char, Deleter>(chars, deleter);
};


#ifdef __cplusplus
extern "C" {
#endif
JavaVM *g_vm = nullptr;
jclass g_NativeBridgeClass = nullptr;          // Global ref to Kotlin NativeBridge
jmethodID g_onNativeComplete = nullptr;        // Cached method ID


/*
 * We attempt to lookup g_nativeBridgeClassName to see if it is loaded in the JVM.
 * If loaded we use callback methods on that class.
 * If the class is not loaded by the JVM we do not attempt to trigger callbacks, this prevents the
 * library having any dependency on RTVA.
 */
const char *g_nativeBridgeClassName = "com/arm/voiceassistant/utils/NativeBridge";

// ------------------------------------------------------------
// Logging helpers
// ------------------------------------------------------------

#define LOGD(fmt, ...)                                              \
        do {                                                                \
            fprintf(stdout, "[%s:%d] " fmt "\n",                            \
                    __FILE__, __LINE__, ##__VA_ARGS__);                     \
            fflush(stdout);                                                 \
        } while (0)


#define LOGE(fmt, ...)                                              \
        do {                                                                \
            fprintf(stdout, "[%s:%d] " fmt "\n",                            \
                    __FILE__, __LINE__, ##__VA_ARGS__);                     \
            fflush(stdout);                                                 \
        } while (0)



JNIEXPORT void JNICALL Java_com_arm_Llm_llmInit(JNIEnv* env,
                                                         jobject /* this */,
                         jstring jsonConfig,
                         jstring sharedLibraryPath) {
    try {
        if (jsonConfig == nullptr) {
                ThrowJavaException(env, "Failed to initialize LLM module, error in config json string ");
                return;
        }
        auto modelCStr = GetUtfChars(env, jsonConfig);
        if (modelCStr.get() == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, jstring to utf conversion failed for config json");
            return;
        }

        if (sharedLibraryPath == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, jstring shared-library-path is null ");
            return;
        }
        auto sharedLibraryPathNative = GetUtfChars(env,sharedLibraryPath);
        if (sharedLibraryPathNative.get() == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, unable to parse shared-library-path into string ");
            return;
    }

        try {
            LlmConfig config(modelCStr.get());
            llm = std::make_unique<LLM>();
            llm->LlmInit(config, sharedLibraryPathNative.get());
        } catch (const std::exception& e) {
            std::string msg = std::string("Failed to create Llm from config : ") + e.what();
            ThrowJavaException(env, msg.c_str());
            return;
        }

        } catch (const std::exception& e) {
        std:: string msg = std::string("Failed to create Llm instance due to error: ") + e.what();
        //  prevents C++ exceptions escaping JNI
        ThrowJavaException(env, msg.c_str());
        return;
    }
}

JNIEXPORT void JNICALL Java_com_arm_Llm_freeLlm(JNIEnv*, jobject)
{
    llm->FreeLlm();
}

JNIEXPORT void JNICALL Java_com_arm_Llm_encode(JNIEnv *env, jobject thiz, jstring jtext, jstring path_to_image,
                                               jboolean is_first_message) {
    try {
        auto textChars = GetUtfChars(env,jtext);
        auto imageChars = GetUtfChars(env,path_to_image);
        std::string text(textChars.get());
        std::string imagePath(imageChars.get());
        LlmChat::Payload payload{text, imagePath, static_cast<bool>(is_first_message)};
        llm->Encode(payload);
    }
    catch (const std::exception& e) {
        ThrowJavaException(env, ("Failed to encode query: " + std::string(e.what())).c_str());
        return;
    }
}

JNIEXPORT jstring JNICALL Java_com_arm_Llm_getNextToken(JNIEnv* env, jobject)
{
    try {
    std::string result = llm->NextToken();
    return env->NewStringUTF(result.c_str());
    }
    catch (const std::exception& e) {
        std::string msg = std::string("Failed to get next token: ") + e.what();
        ThrowJavaException(env,msg.c_str() );
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL Java_com_arm_Llm_getNextTokenCancellable(JNIEnv* env, jobject, jlong operationId)
{
    std::string result = llm->CancellableNextToken(operationId);
    return env->NewStringUTF(result.c_str());
}

JNIEXPORT void JNICALL Java_com_arm_Llm_cancel(JNIEnv* env, jobject, jlong operationId)
{
    llm->Cancel(operationId);
}

JNIEXPORT jfloat JNICALL Java_com_arm_Llm_getEncodeRate(JNIEnv* env, jobject)
{
    float result = llm->GetEncodeTimings();
    return result;
}

JNIEXPORT jfloat JNICALL Java_com_arm_Llm_getDecodeRate(JNIEnv* env, jobject)
{
    float result = llm->GetDecodeTimings();
    return result;
}

JNIEXPORT void JNICALL Java_com_arm_Llm_resetTimings(JNIEnv* env, jobject)
{
    llm->ResetTimings();
}

JNIEXPORT jsize JNICALL Java_com_arm_Llm_getChatProgress(JNIEnv* env, jobject)
{
    return llm->GetChatProgress();
}

JNIEXPORT void JNICALL Java_com_arm_Llm_resetContext(JNIEnv* env, jobject)
{
    try {
    llm->ResetContext();
    }
    catch (const std::exception& e) {
        std::string msg = std::string("Failed to reset context: ") + e.what();
        ThrowJavaException(env,msg.c_str() );
        return;
    }
}

JNIEXPORT jstring JNICALL Java_com_arm_Llm_benchModel(
    JNIEnv* env, jobject, jint nPrompts, jint nEvalPrompts, jint nMaxSeq, jint nRep)
{
    std::string result = llm->BenchModel(nPrompts, nEvalPrompts, nMaxSeq, nRep);
    return env->NewStringUTF(result.c_str());
}

JNIEXPORT jstring JNICALL Java_com_arm_Llm_getFrameworkType(JNIEnv* env, jobject)
{
    std::string frameworkType = llm->GetFrameworkType();
    return env->NewStringUTF(frameworkType.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_arm_Llm_supportsImageInput(JNIEnv *env, jobject thiz) {
    for (const auto &m : llm->SupportedInputModalities()) {
        if (m.find("image") != std::string::npos) {
            return JNI_TRUE;
        }
    }
    return JNI_FALSE;
}

// ------------------------------------------------------------
// JNI exported: nativeCancel(operationId)
// ------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_arm_voiceassistant_utils_LlmBridge_nativeCancel(
        JNIEnv *env,
jobject /*clazz*/,
jlong operationId) {

auto state = findWork(operationId);
if (!state) {
return;
}

state->cancelled.store(true, std::memory_order_release);
}


// Inline JNI–version wrapper with customizable args pointer
#if defined(__ANDROID__)
// Android NDK signature: jint AttachCurrentThread(JNIEnv**, void* args)
#  define JNI_ATTACH_CURRENT_THREAD(vm, penv, args) \
     ( (vm)->AttachCurrentThread( (penv), (args) ) )
#else
// OpenJDK 11+ signature: jint AttachCurrentThread(void** penv, void* args)
#  define JNI_ATTACH_CURRENT_THREAD(vm, penv, args) \
     ( (vm)->AttachCurrentThread( \
         reinterpret_cast<void**>((penv)), (args) \
     ) )
#endif


// Inline JNI–version wrapper with customizable args pointer
#if defined(__ANDROID__)
// Android NDK signature: jint AttachCurrentThread(JNIEnv**, void* args)
#  define JNI_ATTACH_CURRENT_THREAD(vm, penv, args) \
     ( (vm)->AttachCurrentThread( (penv), (args) ) )
#else
// OpenJDK 11+ signature: jint AttachCurrentThread(void** penv, void* args)
#  define JNI_ATTACH_CURRENT_THREAD(vm, penv, args) \
     ( (vm)->AttachCurrentThread( \
         reinterpret_cast<void**>((penv)), (args) \
     ) )
#endif


jint JNI_OnLoad(JavaVM *vm, void *) {
    g_vm = vm;
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    // NOTE: Update this string to match your Kotlin package/class.

    // See comments on g_nativeBridgeClassName for details.
    jclass local = env->FindClass(g_nativeBridgeClassName);
    if (!local) {
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        LOGD("Completion / Cancel handler will not been enabled, check if %s is present and has been loaded", g_nativeBridgeClassName);
        return JNI_VERSION_1_6;
    }
    g_NativeBridgeClass = reinterpret_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    if (!g_NativeBridgeClass) {
        LOGE("JNI_OnLoad: NewGlobalRef failed");
        return JNI_ERR;
    }
    // Signature: (JILjava/lang/String;)V   => long, int, String -> void
    g_onNativeComplete = env->GetStaticMethodID(
            g_NativeBridgeClass,
            "onNativeComplete",
            "(JILjava/lang/String;)V");
    if (!g_onNativeComplete) {
        LOGE("JNI_OnLoad: cannot find onNativeComplete");
        return JNI_ERR;
    }
    LOGD("JNI_OnLoad success");
    return JNI_VERSION_1_6;
}


// ------------------------------------------------------------
// JNI_OnUnload: cleanup global refs (optional but good hygiene)
// ------------------------------------------------------------
void JNI_OnUnload(JavaVM *vm, void *) {
    if (g_NativeBridgeClass == nullptr) {
        return;
    }

    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK) {
        env->DeleteGlobalRef(g_NativeBridgeClass);
    }
    g_onNativeComplete = nullptr;
    g_vm = nullptr;
}


/**
 * @struct EnvScope
 * 
 * @brief Stuct used to track JNI state (attached/detached)
 */
struct EnvScope {
    /**
     * @brief JNI environment pointer for the current thread.
     *
     * After successful construction, this should point to a valid JNIEnv
     * that can be used to call into the JVM.
     */
    JNIEnv* env;

    /**
     * @brief Indicates whether this scope attached the current thread.
     *
     * If true, the destructor will detach the current thread from the JVM.
     * If false, the thread was already attached before this EnvScope was
     * created, and the destructor will not attempt to detach it.
     */
    bool didAttach;

    /**
     * @brief Constructs an EnvScope for the current thread.
     *
     * Ensures that the current native thread is attached to the JVM and
     * initializes the env member. Sets didAttach to true if it had to
     * attach the thread, or false if the thread was already attached.
     */
    EnvScope();

    /**
     * @brief Destroys the EnvScope and optionally detaches the thread.
     *
     * If didAttach is true, the destructor detaches the current thread
     * from the JVM. If didAttach is false, no detachment is performed.
     */
    ~EnvScope();
};

EnvScope::EnvScope() : env(nullptr), didAttach(false) {
    if (g_vm == nullptr) return;  // shouldn't happen
    if (g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (JNI_ATTACH_CURRENT_THREAD(g_vm, &env, nullptr) == JNI_OK) {
            didAttach = true;
        } else {
            env = nullptr;
        }
    }
}

EnvScope::~EnvScope() {
    if (didAttach && g_vm) {
        g_vm->DetachCurrentThread();
    }
}

// ------------------------------------------------------------
// Use this method when the call is complete, can complete call with the following state
// Success / Error / Cancelled
// ------------------------------------------------------------
void deliverCompletion(long operationId, int rc, const std::string &payload) {
    JNIEnv *env;
    bool detach = false;
    if (g_vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        JNI_ATTACH_CURRENT_THREAD(g_vm,&env, nullptr);
        detach = true;
    }

    jclass cls = g_NativeBridgeClass; // assume cached global ref
    jmethodID mid = g_onNativeComplete;
    jstring javaPayload = env->NewStringUTF(payload.c_str());

    env->CallStaticVoidMethod(cls, mid, operationId, rc, javaPayload);
    env->DeleteLocalRef(javaPayload);

    if (detach) g_vm->DetachCurrentThread();
}


#ifdef __cplusplus
}
#endif
