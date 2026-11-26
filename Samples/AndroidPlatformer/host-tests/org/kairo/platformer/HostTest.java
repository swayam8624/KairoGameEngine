package org.kairo.platformer;

import java.util.Arrays;
import java.util.Random;

/** Desktop JVM uses the actual native JNI library, not a Java game model. */
public final class HostTest {
    private static void check(boolean ok,String message) { if(!ok) throw new AssertionError(message); }
    private static int[] state() { int[] s=new int[NativeGame.STATE_SIZE];NativeGame.snapshot(s);return s; }
    private static void ticks(int axis,boolean jump,int n) { for(int i=0;i<n;i++) check(NativeGame.step(axis,jump),"step rejected"); }
    public static void main(String[] args) {
        TouchInput input=new TouchInput();input.update(7,TouchInput.RIGHT);input.update(9,TouchInput.JUMP);
        check(input.axis()==1&&input.jump(),"simultaneous move/jump");input.update(4,TouchInput.LEFT);
        check(input.axis()==0,"opposing directions");input.release(7);check(input.axis()==-1,"stable pointer release");
        input.update(4,TouchInput.NONE);check(input.axis()==0&&input.jump(),"pointer moved outside");input.clear();check(!input.jump(),"cancel");
        NativeGame.reset();ticks(1,false,50);check(state()[3]==1,"death/respawn");
        NativeGame.reset();ticks(1,false,31);ticks(1,true,17);ticks(1,false,80);
        int[] won=state();check(won[4]==1&&won[2]==2&&won[3]==0,"complete objective");
        check(!NativeGame.step(2,false)&&Arrays.equals(won,state()),"invalid input changed state");
        boolean rejected=false;try { NativeGame.snapshot(new int[100]); } catch(IllegalArgumentException expected) { rejected=true; }
        check(rejected,"bad array length accepted");
        Random rng=new Random(724);
        for(int trial=0;trial<100;trial++) {
            int[][] controls=new int[200][2];int[][] reference=new int[200][];NativeGame.reset();
            for(int tick=0;tick<200;tick++) {
                int axis=rng.nextInt(3)-1,jump=rng.nextInt(7)==0?1:0;controls[tick]=new int[]{axis,jump};
                check(NativeGame.step(axis,jump!=0),"random input");reference[tick]=state();
                int[] s=reference[tick];check(s[0]>=0&&s[1]>=0&&s[0]+10<=256&&s[1]+14<=96,"bounds");
                for(int y=s[1]/16;y<=(s[1]+13)/16;y++) for(int x=s[0]/16;x<=(s[0]+9)/16;x++) check(s[5+y*16+x]!='#',"solid penetration");
            }
            NativeGame.reset();for(int tick=0;tick<200;tick++) {
                check(NativeGame.step(controls[tick][0],controls[tick][1]!=0),"replay input");
                check(Arrays.equals(reference[tick],state()),"JNI replay mismatch");
            }
        }
        System.out.println("JNI complete route, multitouch, array bounds and 20000 deterministic replay snapshots passed");
    }
}
