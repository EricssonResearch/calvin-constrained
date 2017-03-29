package ericsson.com.calvin.calvin_constrained;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.os.Bundle;
import android.os.IBinder;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;
import com.google.firebase.messaging.RemoteMessage;

import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;

/**
 * Created by alexander on 2017-01-31.
 */

/**
 * The main service that runs and handles the Calvin runtime.
 */
public class CalvinService extends Service {
    public static final String CLEAR_SERIALIZATION_FILE = "csf";
    public static Calvin calvin;
    Thread calvinThread;
    CalvinDataListenThread cdlt;
    CalvinBroadcastReceiver br;
    public boolean runtimeHasStopped;

    private final String LOG_TAG = "CalvinService";

    @Override
    public void onCreate() {
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startid) {
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);

        // TODO: Move these config params to someplace else
        Bundle intentData = intent.getExtras();
        this.runtimeHasStopped = false;
        String name = "Calvin Android";
        String proxy_uris = "calvinip://192.168.0.108:5000, calvinfcm://773482069446:*";
        String storageDir = getFilesDir().getAbsolutePath();
        calvin = new Calvin(name, proxy_uris, storageDir);

        // Register intent filter for the br
        br = new CalvinBroadcastReceiver(calvin);
        this.registerReceiver(br, intentFilter);

        if (intentData != null && intentData.getBoolean(CLEAR_SERIALIZATION_FILE, false)) {
            calvin.clearSerialization(storageDir);
        }
        calvin.runtimeSerialize = true;
        cdlt = new CalvinDataListenThread(calvin, this);
        calvinThread = new Thread(cdlt);
        calvinThread.start();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        Log.d(LOG_TAG, "Destorying Calvin service");
        this.unregisterReceiver(br);
        if (calvin.nodeState != Calvin.STATE.NODE_STOP) {
            if (calvin.runtimeSerialize)
                calvin.runtimeSerializeAndStop(calvin.node);
            else
                calvin.runtimeStop(calvin.node);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}

/**
 * The thread that runs the Calvin runtime.
 */
class CalvinRuntime implements Runnable {
    private final String LOG_TAG = "Calvin runtime thread";
    Calvin calvin;
    CalvinMessageHandler[] messageHandlers;

    public CalvinRuntime(Calvin calvin, CalvinMessageHandler[] messageHandlers){
        this.calvin = calvin;
        this.messageHandlers = messageHandlers;
    }

    @Override
    public void run() {
        calvin.runtimeStart(calvin.node);
        Log.d(LOG_TAG, "Calvin runtime thread finshed");
    }
}

/**
 * A thread that listens for upstream messages from the Calvin runtime.
 */
class CalvinDataListenThread implements Runnable{
    private Calvin calvin;
    private CalvinMessageHandler[] messageHandlers;
    private final String LOG_TAG = "Android pipelisten";
    private boolean rtStarted = false;
    private CalvinRuntime rt;
    private Thread calvinThread;
    private Context context;

    CalvinMessageHandler[] initMessageHandlers() {
        // Create all message handlers here
        CalvinMessageHandler[] handlers = new CalvinMessageHandler[4];
        handlers[0] = new CalvinMessageHandler() {
            @Override
            public String getCommand() {
                return CalvinMessageHandler.RUNTIME_CALVIN_MSG;
            }

            @Override
            public void handleData(byte[] data) {
                Log.d(LOG_TAG, "Send fcm message with payload");
                if (data.length > 4096) {
                    Log.e(LOG_TAG, "Payload to large for FCM. Not sending");
                } else {
                    Log.d(LOG_TAG, "Payload is: " + new String(data));
                    FirebaseMessaging fm = FirebaseMessaging.getInstance();
                    String id = calvin.getMsgId();
                    RemoteMessage msg = new RemoteMessage.Builder("773482069446@gcm.googleapis.com") // TODO: Load this id from somewhere
                            // .setMessageId("id" + System.currentTimeMillis()) // TODO: Set proper id
                            .setMessageId(id)
                            .addData("msg_type", "payload")
                            .addData("payload", new String(CalvinCommon.base64Encode(data)))
                            .build();
                    calvin.upsreamFCMQueue.add(msg);
                    calvin.sendUpstreamFCMMessages(context);
                }
            }
        };
        handlers[1] = new CalvinMessageHandler() {
            @Override
            public String getCommand() {
                return CalvinMessageHandler.FCM_CONNECT;
            }

            @Override
            public void handleData(byte[] data) {
                Log.d(LOG_TAG, "Send fcm connect message");
                FirebaseMessaging fm = FirebaseMessaging.getInstance();
                String id = calvin.getMsgId();
                RemoteMessage msg = new RemoteMessage.Builder("773482069446@gcm.googleapis.com") // TODO: Load this id from somewhere
                        .setMessageId(id)
                        .addData("msg_type", "set_connect")
                        .addData("connect", "1")
                        .addData("uri", "")
                        .build();
                calvin.upsreamFCMQueue.add(msg);
                calvin.sendUpstreamFCMMessages(context);
            }
        };
        handlers[2] = new CalvinMessageHandler() {
            @Override
            public String getCommand() {
                return CalvinMessageHandler.RUNTIME_STARTED;
            }

            @Override
            public void handleData(byte[] data) {
                Log.d(LOG_TAG, "Android: Runtime started!");
                calvin.writeDownstreamQueue();
            }
        };
        handlers[3] = new CalvinMessageHandler() {
            @Override
            public String getCommand() {
                return CalvinMessageHandler.RUNTIME_STOP;
            }

            @Override
            public void handleData(byte[] data) {
                calvin.nodeState = Calvin.STATE.NODE_STOP;
            }
        };
        return handlers;
    }

    public CalvinDataListenThread(Calvin calvin, Context context){
        this.calvin = calvin;
        this.context = context;
        this.messageHandlers = this.initMessageHandlers();
        calvin.setupCalvinAndInit(calvin.proxyUris, calvin.name, calvin.storageDir);
    }

    public static int get_message_length(byte[] data) {
        int size = 0;
        size = ((data[3] & 0xFF) <<  0) |
                ((data[2] & 0xFF) <<  8) |
                ((data[1] & 0xFF) << 16) |
                ((data[0] & 0xFF) << 24);
        return size;
    }

    @Override
    public void run() {
        Log.d(LOG_TAG, "Starting android pipe listen thread");
        while(true) {
            Log.d(LOG_TAG, "listening for new writes");
            if(!rtStarted) {
                rt = new CalvinRuntime(calvin, initMessageHandlers());
                calvinThread = new Thread(rt);
                calvinThread.start();
                rtStarted = true;
            }

            byte[] raw_data = calvin.readUpstreamData(calvin.node);
            int size = get_message_length(raw_data);

            byte[] cmd = Arrays.copyOfRange(raw_data, 4, 6);
            String cmd_string = new String(cmd);
            byte[] payload = null;
            if(size > 2) {
                payload = Arrays.copyOfRange(raw_data, 6, raw_data.length - 3);
            }
            for(CalvinMessageHandler cmh : this.messageHandlers)
                if (cmh.getCommand().equals(cmd_string)) {
                    if (payload == null) {
                        cmh.handleData(null);
                    } else {
                        cmh.handleData(payload);
                    }
                }
            if (calvin.nodeState == Calvin.STATE.NODE_STOP) {
                // TODO: Close pipes here instead of from calvin constrained
                break;
            }
        }
        Log.d(LOG_TAG, "Calvin data listen thread stoped");
        Intent stopServiceIntent = new Intent(context, CalvinService.class);
        context.stopService(stopServiceIntent);
    }
}
