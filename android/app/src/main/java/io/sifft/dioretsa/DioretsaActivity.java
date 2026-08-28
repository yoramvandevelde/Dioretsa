package io.sifft.dioretsa;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

// This class exists for one call. Asking for minimal post processing is what a
// games console does over HDMI: the television drops its picture processing and
// switches to its own game mode, which is lower latency and, with luck, the end
// of it dimming itself on a picture as dark as this one.
//
// There is no way to ask for it from the manifest, and no way to ask for it from
// native code either: it is a Java method, and it has to run on the UI thread,
// which is where onCreate() already is. Everything else about the app is still
// the shared library NativeActivity loads.
public class DioretsaActivity extends NativeActivity {

    private static final String TAG = "dioretsa";

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);

        // Nothing should dim or fall asleep in the middle of a run. This is the
        // inactivity timeout only; a television that dims because the picture
        // itself is dark is doing something else entirely.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Log.i(TAG, "Minimal post processing wants Android 11; this is API " + Build.VERSION.SDK_INT);
            return;
        }

        getWindow().setPreferMinimalPostProcessing(true);

        // Worth knowing when the picture does not change: it tells a television
        // that cannot do this apart from one that simply would not.
        Display display = getDisplay();
        Log.i(TAG, "Minimal post processing asked for; display supports it: "
                + ((display != null) && display.isMinimalPostProcessingSupported()));
    }
}
