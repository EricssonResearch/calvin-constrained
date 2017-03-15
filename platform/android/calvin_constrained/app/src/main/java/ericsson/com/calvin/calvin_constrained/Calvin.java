package ericsson.com.calvin.calvin_constrained;

import android.content.Intent;
import android.util.Log;

import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedDeque;

/**
 * Created by alexander on 2017-01-30.
 */

public class Calvin {

    private final String LOG_TAG = "Native Calvin";
    public long node = 0;
    private long msg_counter = 1;
    public boolean runtimeSerialize = false;
    public String name, proxyUris, storageDir;
    public Queue<byte[]> downstreamQueue;
    public STATE nodeState;

    static {
        System.loadLibrary("calvin_constrained");
    }

    public enum STATE {
        NODE_DO_START,
        NODE_PENDING,
        NODE_STARTED,
        NODE_STOP,
        NODE_UNKNOW
    }

    public Calvin(String name, String proxyUris, String storageDir) {
        this.name = name;
        this.proxyUris = proxyUris;
        this.storageDir = storageDir;
        this.runtimeSerialize = false;
        downstreamQueue = new ConcurrentLinkedDeque<byte[]>();
    }

    public synchronized boolean writeDownstreamQueue() {
        Log.d(LOG_TAG, "Write downstream queue of size: " + downstreamQueue.size());
        if (getNodeState() != STATE.NODE_STOP) {
            while(!downstreamQueue.isEmpty()) {
                Log.d(LOG_TAG, "Write data from downstream queue");
                runtimeCalvinPayload(downstreamQueue.poll(), node);
            }
        } else {
            return false;
        }
        return true;
    }

    public void setupCalvinAndInit(String name, String proxyUris, String storageDir) {
        this.node = this.runtimeInit(name, proxyUris, storageDir);
    }

    public synchronized String getMsgId() {
        this.msg_counter ++;
        return this.msg_counter + "_" + System.currentTimeMillis();
    }

    public Calvin.STATE getNodeState() {
        switch(this.getNodeState(this.node)) {
            case 0:
                nodeState = STATE.NODE_DO_START;
                return nodeState;
            case 1:
                nodeState = STATE.NODE_PENDING;
                return nodeState;
            case 2:
                nodeState = STATE.NODE_STARTED;
                return nodeState;
            case 3:
                nodeState = STATE.NODE_STOP;
                return nodeState;
        }
        Log.e(LOG_TAG, "Node state unknown");
        nodeState = STATE.NODE_UNKNOW;
        return nodeState;
    }

    public native void runtimeStart(long node);
    public native String runtimeStop(long node);
    public native long runtimeInit(String proxy_uris, String name, String storageDir);
    public native byte[] readUpstreamData(long node);
    public native void runtimeCalvinPayload(byte[] payload, long node);
    public native void fcmTransportConnected(long node);
    public native void runtimeSerializeAndStop(long node);
    public native void clearSerialization(String filedir);
    private native int getNodeState(long node);
}