#include <jni.h>
#include <array>
#include <bit>
#include <cstdint>

// Reuse the same scalar C ABI as WebAssembly. The mobile host deliberately
// owns a single fixed-level session, serialized by NativeGame's class lock.
extern "C" {
    void game_reset();
    int game_step(int,int);
    int game_tile(int,int);
    int game_x();
    int game_y();
    unsigned game_coins();
    unsigned game_deaths();
    int game_won();
}

extern "C" JNIEXPORT void JNICALL Java_org_kairo_platformer_NativeGame_reset(JNIEnv*,jclass)
{game_reset();}
extern "C" JNIEXPORT jboolean JNICALL Java_org_kairo_platformer_NativeGame_step(JNIEnv*,jclass,jint axis,jboolean jump)
{return game_step(axis,jump?1:0)?JNI_TRUE:JNI_FALSE;}

// Copy the complete state in one JNI call, into Java-owned storage. Validate
// the destination size first; no native pointers or allocation ownership
// cross the boundary. Counts use their original unsigned 32-bit bit pattern.
extern "C" JNIEXPORT void JNICALL Java_org_kairo_platformer_NativeGame_snapshot(JNIEnv* env,jclass,jintArray out)
{
    if(!out||env->GetArrayLength(out)!=101)
    {
        if(auto type=env->FindClass("java/lang/IllegalArgumentException"))env->ThrowNew(type,"Expected 101 state integers");
        return;
    }
    std::array<jint,101> data{};
    data[0]=game_x();data[1]=game_y();
    data[2]=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(game_coins()));
    data[3]=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(game_deaths()));
    data[4]=game_won();
    for(int y=0;y<6;++y)for(int x=0;x<16;++x)data[5+y*16+x]=game_tile(x,y);
    env->SetIntArrayRegion(out,0,static_cast<jsize>(data.size()),data.data());
}
