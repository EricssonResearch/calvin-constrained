package ericsson.com.calvin.calvin_constrained;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.support.annotation.Nullable;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;
import com.google.firebase.messaging.RemoteMessage;

import java.util.Arrays;

/**
 * Created by alexander on 2017-01-31.
 */

public class CalvinService extends Service{
    public static Calvin calvin;
    Thread calvinThread;
    CalvinDataListenThread cdlt;

    private final String LOG_TAG = "CalvinService";

    @Override
    public void onCreate() {
        calvin = new Calvin();
        cdlt = new CalvinDataListenThread(calvin);
        calvinThread = new Thread(cdlt);
        calvinThread.start();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startid){
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        Log.d(LOG_TAG, "Destorying Calvin service");
        calvin.runtimeStop(calvin.node);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}

class CalvinRuntime implements Runnable {
    Calvin calvin;
    CalvinMessageHandler[] messageHandlers;

    public CalvinRuntime(Calvin calvin, CalvinMessageHandler[] messageHandlers){
        this.calvin = calvin;
        this.messageHandlers = messageHandlers;
    }

    @Override
    public void run() {
        calvin.runtimeStart(calvin.node);
    }
}

class CalvinDataListenThread implements Runnable{
    private Calvin calvin;
    private CalvinMessageHandler[] messageHandlers;
    private final String LOG_TAG = "Android pipelisten";
    private final int BUFFER_SIZE = 512;
    private boolean rtStarted = false;
    private CalvinRuntime rt;
    private Thread calvinThread;

    CalvinMessageHandler[] initMessageHandlers() {
        // Create all message handlers here
        CalvinMessageHandler[] handlers = new CalvinMessageHandler[2];
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
                    fm.send(msg);
                    Log.d(LOG_TAG, "Sent fcm message!");
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
                fm.send(msg);
                Log.d(LOG_TAG, "Sent fcm message!");
            }
        };

        return handlers;
    }

    public CalvinDataListenThread(Calvin calvin){
        this.calvin = calvin;
        this.messageHandlers = this.initMessageHandlers();
        String name = "Calvin Android";
        String proxy_uris = "calvinfcm://123:asd";
        calvin.setupCalvinAndInit(proxy_uris, name);
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
        }
    }
}
