package ericsson.com.calvin.calvin_constrained;

import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;
import com.google.firebase.messaging.RemoteMessage;

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
    public Queue<RemoteMessage> upsreamFCMQueue;
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

	/**
	 * Create a Calvin object that binds Java to the native methods
     * @param name The name of the runtime to start
     * @param proxyUris A comma separated list of proxy uris
     * @param storageDir The directory to use for storage when serializing the runtime
     */
    public Calvin(String name, String proxyUris, String storageDir) {
        this.name = name;
        this.proxyUris = proxyUris;
        this.storageDir = storageDir;
        this.runtimeSerialize = false;
        downstreamQueue = new ConcurrentLinkedDeque<byte[]>();
        upsreamFCMQueue = new ConcurrentLinkedDeque<RemoteMessage>();
    }

	/**
	 * Trigger a write of the queue for messages to be sent to the Calvin runtime.
     * @return
     */
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

	/**
	 * Write messages in the queue for upstream FCM messages
     * @param context
     * @return
     */
    public synchronized boolean sendUpstreamFCMMessages(Context context) {
        ConnectivityManager cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo ni = cm.getActiveNetworkInfo();
        if (ni != null && ni.isConnected()) {
            FirebaseMessaging fm = FirebaseMessaging.getInstance();
            while(!upsreamFCMQueue.isEmpty()) {
                Log.d(LOG_TAG, "Sending fcm message");
                fm.send(upsreamFCMQueue.poll());
            }
            return true;
        } else {
            Log.d(LOG_TAG, "Had no internet, could not send fcm messages");
        }
        return false;
    }

	/**
     * Setup Calvin, create a reference to the node and initiate the runtime.
     * @param name The name of the runtime to start
     * @param proxyUris A list of comma separated proxy uris.
     * @param storageDir The directory to use for storage when serializing the runtime
     */
    public void setupCalvinAndInit(String name, String proxyUris, String storageDir) {
        this.node = this.runtimeInit(name, proxyUris, storageDir);
    }

	/**
	 * Generate a unique message id to be used when sending FCM upstream messages
     * @return The unique id
     */
    public synchronized String getMsgId() {
        this.msg_counter ++;
        return this.msg_counter + "_" + System.currentTimeMillis();
    }

	/**
	 * Get the state of the runtiem.
     * @return The state
     */
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

	/**
	 * Native method call to start the runtime
     * @param node The node object reference
     */
    public native void runtimeStart(long node);

	/**
	 * Native method to send a message to Calvin to stop the runtime.
     * @param node The node
     * @return
     */
    public native String runtimeStop(long node);

	/**
	 * Native method to initiate and create the node. To actually init the node, use the setupCalvinAndInit method instead.
     * @param proxy_uris A list of comma separated proxy uris.
     * @param name The directory to use for storage when serializing the runtime
     * @param storageDir The name of the runtime to start
     * @return The newly created node.
     */
    public native long runtimeInit(String proxy_uris, String name, String storageDir);

	/**
	 * Native method to read data from the upstream pipe.
     * @param node The node
     * @return
     */
    public native byte[] readUpstreamData(long node);

	/**
	 * Native method to send downstream data to Calvin.
     * @param payload The payload to write.
     * @param node
     */
    public native void runtimeCalvinPayload(byte[] payload, long node);

	/**
	 * Native method to indicate that the FCM transport is connected
     * @param node The node
     */
    public native void fcmTransportConnected(long node);

	/**
	 * Native method to indicate that the runtime should serialize and then stop.
     * @param node The node
     */
    public native void runtimeSerializeAndStop(long node);

	/**
	 * Native method to remove the file used for serialization if it exists.
     * @param filedir The file direcotry of which the file is located.
     */
    public native void clearSerialization(String filedir);

	/**
	 * Native method to trigger Calvin to do a reconnect since there was a change in connectivity.
     * @param node The node.
     */
    public native void triggerConnectivityChange(long node);

	/**
	 * Native method to get the state of the node. Use the Calvin.STATE getNodeState() instead of this.
     * @param node The node.
     * @return
     */
    private native int getNodeState(long node);
}