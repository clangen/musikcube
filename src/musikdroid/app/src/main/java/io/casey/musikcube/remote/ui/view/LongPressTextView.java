package io.casey.musikcube.remote.ui.view;

import android.content.Context;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

public class LongPressTextView extends TextView {
    private static final int TICK_START_DELAY = 700;
    private static final int MINIMUM_TICK_DELAY = 100;
    private static final int TICK_DELTA = 100;
    private static final int FIRST_TICK_DELAY = 200;

    public interface OnTickListener {
        void onTick(final View view);
    }

    private int tickDelay = 0;
    private int ticksFired = 0;
    private boolean isDown;
    private Handler handler = new Handler();
    private OnTickListener onTickListener;
    private View.OnClickListener onClickListener;

    public LongPressTextView(Context context) {
        super(context);
        init();
    }

    public LongPressTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public LongPressTextView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    public void setOnTickListener(OnTickListener onTickListener) {
        this.onTickListener = onTickListener;
        this.setClickable(onTickListener != null);
    }

    @Override
    public void setOnClickListener(OnClickListener l) {
        this.onClickListener = l;
    }

    private void init() {
        super.setOnClickListener(onClickProxy);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            isDown = true;
            ticksFired = 0;
            tickDelay = TICK_START_DELAY;
            handler.removeCallbacks(tickRunnable);
            handler.postDelayed(tickRunnable, FIRST_TICK_DELAY);
        }
        else if (event.getAction() == MotionEvent.ACTION_UP) {
            handler.removeCallbacks(tickRunnable);
            ticksFired = 0;
            isDown = false;
        }

        return super.onTouchEvent(event);
    }

    private Runnable tickRunnable = new Runnable() {
        @Override
        public void run() {
            if (isDown) {
                if (onTickListener != null) {
                    onTickListener.onTick(LongPressTextView.this);
                }

                tickDelay = Math.max(MINIMUM_TICK_DELAY, tickDelay - TICK_DELTA);
                handler.postDelayed(tickRunnable, tickDelay);
            }
        }
    };

    private View.OnClickListener onClickProxy = new OnClickListener() {
        @Override
        public void onClick(View view) {
            if (onTickListener == null && onClickListener != null) {
                onClickListener.onClick(view);
            }
            else if (onTickListener != null && ticksFired == 0) {
                onTickListener.onTick(view);
            }
        }
    };
}
