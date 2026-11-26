package org.kairo.platformer;

import android.app.Activity;
import android.os.Bundle;
import android.os.SystemClock;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.Log;
import android.view.Choreographer;
import android.view.MotionEvent;
import android.view.View;
import java.util.Arrays;

/** Native Android presentation. Java owns the view and input/lifecycle;
 * C++ owns movement, collisions, collectibles and the win state. */
public final class MainActivity extends Activity {
    private GameView game;
    private boolean smoke;
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        smoke=getIntent().getBooleanExtra("smoke",false);
        NativeGame.reset();
        game=new GameView();setContentView(game);
        if(smoke)game.post(()->game.smoke());
    }
    @Override public void onResume() { super.onResume();if(!smoke)game.start(); }
    @Override public void onPause() { game.stop();super.onPause(); }

    private final class GameView extends View implements Choreographer.FrameCallback {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        private final TouchInput input=new TouchInput();
        private final int[] state=new int[NativeGame.STATE_SIZE];
        private boolean running;
        private long previous;
        private double accumulator;
        GameView() { super(MainActivity.this);setFocusable(true);setContentDescription("Kairo platformer game"); }
        void start() { if(running)return;running=true;previous=0;accumulator=0;Choreographer.getInstance().postFrameCallback(this); }
        void stop() { running=false;Choreographer.getInstance().removeFrameCallback(this);clearInput(); }
        private void clearInput() { input.clear();previous=0;accumulator=0; }
        @Override public void onWindowFocusChanged(boolean focused) { super.onWindowFocusChanged(focused);if(!focused)clearInput(); }
        @Override protected void onSizeChanged(int w,int h,int oldw,int oldh) { clearInput(); }
        // Cap elapsed time and advance only full fixed 60 Hz simulation ticks.
        @Override public void doFrame(long now) {
            if(!running)return;
            if(hasWindowFocus()&&previous!=0)accumulator+=Math.min(.1,Math.max(0,(now-previous)*1e-9));
            previous=now;
            while(accumulator>=1.0/60) { tick();accumulator-=1.0/60; }
            invalidate();Choreographer.getInstance().postFrameCallback(this);
        }
        private void tick() { if(!NativeGame.step(input.axis(),input.jump()))throw new IllegalStateException("Native input rejected"); }
        private void rect(Canvas c,float x,float y,float w,float h,int color) {
            paint.setColor(color);c.drawRect(x,y,x+w,y+h,paint);
        }
        @Override protected void onDraw(Canvas c) {
            super.onDraw(c);NativeGame.snapshot(state);
            float w=getWidth(),h=getHeight(),scale=Math.min(w/256f,h*.60f/96f);
            float ox=(w-256*scale)/2,oy=(h*.60f-96*scale)/2;
            c.drawColor(0xff0d1726);
            for(int y=0;y<6;y++)for(int x=0;x<16;x++) {
                int tile=state[5+y*16+x],color;
                if(tile=='#')color=0xff52657b;else if(tile=='C')color=0xffffc94a;
                else if(tile=='H')color=0xffe75b58;else if(tile=='G')color=0xff42b879;else continue;
                int inset=tile=='C'?5:0;
                rect(c,ox+(16*x+inset)*scale,oy+(16*y+inset)*scale,(16-2*inset)*scale,(16-2*inset)*scale,color);
            }
            rect(c,ox+state[0]*scale,oy+state[1]*scale,10*scale,14*scale,0xff50c9fc);
            paint.setTextSize(Math.min(w/25,h*.045f));paint.setColor(0xffe6edf5);paint.setTextAlign(Paint.Align.CENTER);
            c.drawText(state[4]!=0?"Level complete! Tap status to restart":"Crystals "+state[2]+"/2   Deaths "+Integer.toUnsignedString(state[3]),w/2,h*.68f,paint);
            String[] labels={"LEFT","RIGHT","JUMP"};
            for(int i=0;i<3;i++) {
                boolean active=i==0?input.axis()<0:i==1?input.axis()>0:input.jump();
                rect(c,w*i/3+4,h*.75f,w/3-8,h*.22f,active?0xff41729d:0xff23354c);
                paint.setColor(0xffe6edf5);c.drawText(labels[i],w*(i+.5f)/3,h*.88f,paint);
            }
        }
        private int control(float x,float y) {
            if(x<0||x>=getWidth()||y<getHeight()*.75f||y>=getHeight())return TouchInput.NONE;
            return Math.min(2,(int)(3*x/getWidth()))+1;
        }
        @Override public boolean onTouchEvent(MotionEvent e) {
            int action=e.getActionMasked(),index=e.getActionIndex();
            if(action==MotionEvent.ACTION_CANCEL){input.clear();invalidate();return true;}
            if(action==MotionEvent.ACTION_DOWN&&e.getY()<getHeight()*.75f) {
                NativeGame.snapshot(state);
                if(state[4]!=0){NativeGame.reset();input.clear();}
            }
            // Update all surviving pointers by ID, not mutable pointer index.
            for(int i=0;i<e.getPointerCount();i++) {
                int id=e.getPointerId(i);
                if((action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP)&&i==index)input.release(id);
                else input.update(id,control(e.getX(i),e.getY(i)));
            }
            if(action==MotionEvent.ACTION_UP){input.clear();performClick();}
            invalidate();return true;
        }
        @Override public boolean performClick(){super.performClick();return true;}

        // Emulator fixture routes synthetic Android MotionEvents through
        // dispatchTouchEvent. It does not bypass the real pointer handler.
        private void touch(int action,int[] ids,int[] controls) {
            MotionEvent.PointerProperties[] props=new MotionEvent.PointerProperties[ids.length];
            MotionEvent.PointerCoords[] coords=new MotionEvent.PointerCoords[ids.length];
            for(int i=0;i<ids.length;i++) {
                props[i]=new MotionEvent.PointerProperties();props[i].id=ids[i];props[i].toolType=MotionEvent.TOOL_TYPE_FINGER;
                coords[i]=new MotionEvent.PointerCoords();coords[i].x=getWidth()*(controls[i]-.5f)/3;coords[i].y=getHeight()*.85f;
                coords[i].pressure=1;coords[i].size=1;
            }
            long now=SystemClock.uptimeMillis();
            MotionEvent e=MotionEvent.obtain(now,now,action,ids.length,props,coords,0,0,1,1,0,0,android.view.InputDevice.SOURCE_TOUCHSCREEN,0);
            try { if(!dispatchTouchEvent(e))throw new IllegalStateException("Touch event rejected"); } finally { e.recycle(); }
        }
        private int[] pixels() {
            Bitmap bitmap=Bitmap.createBitmap(getWidth(),getHeight(),Bitmap.Config.ARGB_8888);
            try {draw(new Canvas(bitmap));int[] result=new int[getWidth()*getHeight()];bitmap.getPixels(result,0,getWidth(),0,0,getWidth(),getHeight());return result;}
            finally {bitmap.recycle();}
        }
        private void ticks(int n){for(int i=0;i<n;i++)tick();}
        void smoke() {
            try {
                if(getWidth()==0||getHeight()==0){postDelayed(this::smoke,50);return;}
                int[] before=pixels();
                touch(MotionEvent.ACTION_DOWN,new int[]{7},new int[]{TouchInput.RIGHT});ticks(50);
                touch(MotionEvent.ACTION_UP,new int[]{7},new int[]{TouchInput.RIGHT});NativeGame.snapshot(state);
                if(state[3]!=1)throw new IllegalStateException("Respawn failed");
                NativeGame.reset();
                touch(MotionEvent.ACTION_DOWN,new int[]{7},new int[]{TouchInput.RIGHT});ticks(31);
                touch(MotionEvent.ACTION_POINTER_DOWN|(1<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),new int[]{7,9},new int[]{TouchInput.RIGHT,TouchInput.JUMP});ticks(17);
                touch(MotionEvent.ACTION_POINTER_UP|(1<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),new int[]{7,9},new int[]{TouchInput.RIGHT,TouchInput.JUMP});ticks(80);
                touch(MotionEvent.ACTION_UP,new int[]{7},new int[]{TouchInput.RIGHT});
                NativeGame.snapshot(state);
                if(state[4]!=1||state[2]!=2||state[3]!=0||Arrays.equals(before,pixels()))throw new IllegalStateException("Objective/pixel gate failed");
                invalidate();Log.i("KairoMobile","SMOKE_PASSED");
            } catch(RuntimeException error){Log.e("KairoMobile","SMOKE_FAILED",error);throw error;}
        }
    }
}
