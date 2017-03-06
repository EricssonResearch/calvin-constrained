package ericsson.com.calvin.calvin_constrained;

/**
 * Created by alexander on 2017-01-30.
 */

public class Calvin {

    static {
        System.loadLibrary("calvin_constrained");
    }

    public native void runtimeStart();
    public native String runtimeStop();
    public native void runtimeInit();
    public native byte[] readUpstreamData();
    public native void runtimeCalvinPayload(byte[] payload);
    public native void fcmTransportConnected();
}