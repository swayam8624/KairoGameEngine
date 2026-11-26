package org.kairo.platformer;

import java.util.HashMap;
import java.util.Map;

/** Pointer IDs are stable across Android pointer-index changes. A pointer
 * owns its current control until it moves, lifts, or is cancelled. */
public final class TouchInput {
    public static final int NONE=0, LEFT=1, RIGHT=2, JUMP=3;
    private final Map<Integer,Integer> pointers = new HashMap<>();
    public void update(int id, int control) {
        if (id<0 || control<NONE || control>JUMP) throw new IllegalArgumentException("Invalid pointer/control");
        if (control==NONE) pointers.remove(id);
        else pointers.put(id,control);
    }
    public void release(int id) { pointers.remove(id); }
    public void clear() { pointers.clear(); }
    public int axis() { return (pointers.containsValue(RIGHT)?1:0)-(pointers.containsValue(LEFT)?1:0); }
    public boolean jump() { return pointers.containsValue(JUMP); }
}
