package ericsson.com.calvin.calvin_constrained;

import android.util.Log;

/**
 * Created by alexander on 2017-01-30.
 */

public class Calvin {

    private final String LOG_TAG = "Native Calvin";
    public long node = 0;
    private long msg_counter = 1;
    public boolean runtimeSerialize = false;
    public String name, proxyUris, storageDir;

    static {
        System.loadLibrary("calvin_constrained");
    }

    public Calvin(String name, String proxyUris, String storageDir) {
        this.name = name;
        this.proxyUris = proxyUris;
        this.storageDir = storageDir;
        this.runtimeSerialize = false;
    }

    public void setupCalvinAndInit(String name, String proxyUris, String storageDir) {
        this.node = this.runtimeInit(name, proxyUris, storageDir);
    }

    public synchronized String getMsgId() {
        this.msg_counter ++;
        return this.msg_counter + "_" + System.currentTimeMillis();
    }

    public native void runtimeStart(long node);
    public native String runtimeStop(long node);
    public native long runtimeInit(String proxy_uris, String name, String storageDir);
    public native byte[] readUpstreamData(long node);
    public native void runtimeCalvinPayload(byte[] payload, long node);
    public native void fcmTransportConnected(long node);
    public native void runtimeSerializeAndStop(long node);
}