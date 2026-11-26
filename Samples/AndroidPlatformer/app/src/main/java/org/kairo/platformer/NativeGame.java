package org.kairo.platformer;

/** Single-session JNI facade. Class synchronization serializes all access
 * to the shared C++ fixed-level adapter, including snapshot copies. */
public final class NativeGame {
    public static final int STATE_SIZE = 101;
    static { System.loadLibrary("kairo_mobile"); }
    private NativeGame() {}
    public static synchronized native void reset();
    public static synchronized native boolean step(int axis, boolean jump);
    public static synchronized native void snapshot(int[] out);
}
