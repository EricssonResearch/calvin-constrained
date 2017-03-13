package ericsson.com.calvin.calvin_constrained;

import android.util.Log;

/**
 * Created by alexander on 2017-01-30.
 */

public class Calvin {

    private final String LOG_TAG = "Native Calvin";
    public long node = 0;

    static {
        System.loadLibrary("calvin_constrained");
    }

    public void setupCalvinAndInit(String name, String proxy_uris) {
        this.node = this.runtimeInit(name, proxy_uris);
    }

    public native void runtimeStart(long node);
    public native String runtimeStop(long node);
    public native long runtimeInit(String proxy_uris, String name);
    public native byte[] readUpstreamData(long node);
    public native void runtimeCalvinPayload(byte[] payload, long node);
    public native void fcmTransportConnected(long node);
}
