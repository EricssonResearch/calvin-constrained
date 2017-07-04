package ericsson.com.calvin.calvin_constrained;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;
import com.google.firebase.messaging.RemoteMessage;

import org.msgpack.core.MessageBufferPacker;
import org.msgpack.core.MessagePack;
import org.msgpack.core.MessageUnpacker;
import org.msgpack.value.MapValue;
import org.msgpack.value.Value;
import org.msgpack.value.ValueType;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * Created by alexander on 2017-01-31.
 */

/**
 * The main service that runs and handles the Calvin runtime.
 */
public class CalvinService extends Service {
    public final int REGISTER_CALVINSYS = 1, DESTORY_CALVINSYS = 2, CALVINSYS_DATA = 3, CALVINSYS_ACK = 4;
    public final int INIT_CALVINSYS = 5;

    public static final String CLEAR_SERIALIZATION_FILE = "csf";
    public static Calvin calvin;
    Thread calvinThread;
    CalvinDataListenThread cdlt;
    CalvinBroadcastReceiver br;
    public boolean runtimeHasStopped;
    Messenger msg = new Messenger(new CalvinMessengerHandler());
    private final String LOG_TAG = "CalvinService";
    Map<String, ExternalCalvinSys> clients;

    enum ExternalClientState {
        ENABLED,
        PENDING,
        DISABLED
    };

    class ExternalCalvinSys {
        ExternalClientState state;
        String name;
        Messenger outgoing;
        int uid;
    }

    class CalvinMessengerHandler extends Handler {
        @Override
        public void handleMessage(Message message) {
            Log.d(LOG_TAG, "Received message");
            Bundle data;
            switch(message.what) {
                case REGISTER_CALVINSYS:
                    data = message.getData();
                    String name = data.getString("name", null);

                    ExternalCalvinSys sys = new ExternalCalvinSys();
                    sys.state = ExternalClientState.ENABLED;
                    sys.name = name;
                    sys.outgoing = message.replyTo;
                    sys.uid = message.sendingUid;
                    clients.put(sys.name, sys);

                    calvin.registerExternalCalvinsys(calvin.node, name);

                    // Send reply message
                    Message pack = Message.obtain(null, CALVINSYS_ACK, 0, 0);
                    try {
                        sys.outgoing.send(pack);
                    } catch (RemoteException e) {
                        e.printStackTrace();
                    }
                    break;
                case CALVINSYS_DATA:
                    sendDownstreamCalvinSysMessage(message);
                    break;
            }
        }
    }

    private void sendDownstreamCalvinSysMessage(Message message) {
        Bundle data = message.getData();
        String sysName = data.getString("calvinsys");
        String command = data.getString("command");
        byte[] payload = data.getByteArray("payload");
        if (sysName == null || command == null) {
            Log.e(LOG_TAG, "Error when sending data to sys handler. The calvinsys must contain the name of the registered sys.");
            return;
        }
        MessageBufferPacker packer = MessagePack.newDefaultBufferPacker();
        try {
            // TODO: Future work, instead of getting a msgpack coded data object, parse it to/from a Bundle
            packer.packMapHeader(3);

            packer.packString("calvinsys");
            packer.packString(sysName+"\0");

            packer.packString("command");
            packer.packString(command+"\0");

            packer.packString("payload");
            packer.packBinaryHeader(payload.length);
            packer.writePayload(payload);

            packer.close();
            calvin.writeCalvinsysPayload(packer.toByteArray(), calvin.node);

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    synchronized public boolean sendInitExternalCalvinsys(String name) {
        Log.d(LOG_TAG, "Send init to external calvinsys: " + name);
        ExternalCalvinSys sys = clients.get(name);
        if (sys == null) {
            Log.d(LOG_TAG, "calvinsys not found");
            return false;
        }
        Message pack = Message.obtain(null, INIT_CALVINSYS);
        try {
            sys.outgoing.send(pack);
        } catch (RemoteException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    synchronized public boolean sendDestroyExternalCalvinsys(String name) {
        Log.d(LOG_TAG, "Send init to external calvinsys: " + name);
        ExternalCalvinSys sys = clients.get(name);
        if (sys == null) {
            Log.d(LOG_TAG, "calvinsys not found");
            return false;
        }
        Message pack = Message.obtain(null, DESTORY_CALVINSYS);
        try {
            sys.outgoing.send(pack);
        } catch (RemoteException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    synchronized void sendPayloadExternalCalvinsys(byte[] data) {
        MessageUnpacker unpacker = MessagePack.newDefaultUnpacker(data);
        try {
            Bundle bundle = new Bundle();
            if(unpacker.hasNext()) {
                Value v = unpacker.unpackValue();
				if(v.getValueType() == ValueType.MAP) {
                        MapValue map = v.asMapValue();
                        Set<Map.Entry<Value, Value>> mapSet = map.entrySet();
                        for(Map.Entry<Value, Value> e : mapSet) {
                            if (e.getKey().getValueType() == ValueType.STRING) {
                                ValueType a = e.getValue().getValueType();
                                String key = e.getKey().asStringValue().asString();
                                if(a == ValueType.STRING && key.startsWith("command")) {
                                    String value = e.getValue().asStringValue().asString();
                                    bundle.putString(key, value.substring(0, value.length()-1));
                                }else if(a == ValueType.STRING && key.startsWith("calvinsys")) {
                                    String value = e.getValue().asStringValue().asString();
                                    bundle.putString(key, value.substring(0, value.length()-1));
                                } else if(a == ValueType.BINARY && key.startsWith("payload")) {
                                    byte[] value = e.getValue().asBinaryValue().asByteArray();
                                    bundle.putByteArray(key, value);
                                }
                            } else {
                                Log.e(LOG_TAG, "Found non string key in map, ignoring");
                            }
                        }
                } else {
                    Log.e(LOG_TAG, "Was not map");
                }
			} else {
                Log.e(LOG_TAG, "Did not find any data in msgpack...");
            }

            Message msg = Message.obtain(null, 3, 0, 0);
            msg.setData(bundle);
            ExternalCalvinSys sys = clients.get(bundle.getString("calvinsys"));
            sys.outgoing.send(msg);
        } catch (IOException e) {
            e.printStackTrace();
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onCreate() {
        if (clients == null)
            clients = new HashMap<String, ExternalCalvinSys>();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startid) {
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);

        // TODO: Move these config params to someplace else
        Bundle intentData = null;
        if (intent != null)
            intentData = intent.getExtras();
        this.runtimeHasStopped = false;
        String name = intentData.getString("rt_name", "Calvin Android");
        String proxy_uris = intentData.getString("rt_uris", "ssdp");
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
        Log.d(LOG_TAG, "onbind");
        return msg.getBinder();
    }
}

/**
 * The thread that runs the Calvin runtime.
 */
class CalvinRuntime implements Runnable {
    private final String LOG_TAG = "Calvin runtime thread";
    Calvin calvin;
    CalvinMessageHandler[] messageHandlers;
    Object threadLock;

    public CalvinRuntime(Calvin calvin, CalvinMessageHandler[] messageHandlers, Object threadLock){
        this.calvin = calvin;
        this.messageHandlers = messageHandlers;
        this.threadLock = threadLock;
    }

    @Override
    public void run() {
        synchronized (threadLock) {
            calvin.setupCalvinAndInit(calvin.proxyUris, calvin.name, calvin.storageDir);
            threadLock.notify();
            calvin.runtimeStart(calvin.node);
            Log.d(LOG_TAG, "Calvin runtime thread finshed");
        }
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
    private CalvinService calvinService;
    private Thread calvinThread;
    Object thread_lock = new Object();

    CalvinMessageHandler[] initMessageHandlers() {
        // Create all message handlers here
        CalvinMessageHandler[] handlers = new CalvinMessageHandler[7];
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
                    calvin.sendUpstreamFCMMessages(calvinService);
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
                calvin.sendUpstreamFCMMessages(calvinService);
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
        handlers[4] = new CalvinMessageHandler() {

            @Override
            public String getCommand() {
                return CalvinMessageHandler.INIT_EXTERNAL_CALVINSYS;
            }

            @Override
            public void handleData(byte[] data) {
                String name = new String(data);
                if(!calvinService.sendInitExternalCalvinsys(name)) {
                    Log.e(LOG_TAG, "Could not sent init to external calvinsys");
                }
            }
        };
        handlers[5] = new CalvinMessageHandler() {
            @Override
            public String getCommand() {
                return CalvinMessageHandler.PAYLOAD_EXTERNAL_CALVINSYS;
            }

            @Override
            public void handleData(byte[] data) {
                calvinService.sendPayloadExternalCalvinsys(data);
            }
        };
        handlers[6] = new CalvinMessageHandler() {
            @Override
            public String getCommand() {
                return CalvinMessageHandler.DESTROY_EXTERNAL_CALVINSYS;
            }

            @Override
            public void handleData(byte[] data) {
                String name = new String(data);
                if(!calvinService.sendDestroyExternalCalvinsys(name)) {
                    Log.e(LOG_TAG, "Could not destory external calvinsys " + new String(name));
                }
            }
        };
        return handlers;
    }

    public CalvinDataListenThread(Calvin calvin, CalvinService service){
        this.calvin = calvin;
        this.messageHandlers = this.initMessageHandlers();
        this.calvinService = service;
        //calvin.setupCalvinAndInit(calvin.proxyUris, calvin.name, calvin.storageDir);
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
                rt = new CalvinRuntime(calvin, initMessageHandlers(), thread_lock);
                calvinThread = new Thread(rt);
                rtStarted = true;

                synchronized (thread_lock) {
                    try {
                        calvinThread.start();
                        thread_lock.wait();
                    } catch (InterruptedException e) {
                        Log.e(LOG_TAG, "Could not call wait for calvin runtime to init and create node");
                    }
                }
            }

            byte[] raw_data = calvin.readUpstreamData(calvin.node);
            if (raw_data == null) {
              Log.e(LOG_TAG, "Failed read upstream data");
            } else {
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
        }
        Log.d(LOG_TAG, "Calvin data listen thread stoped");
        Intent stopServiceIntent = new Intent(calvinService, CalvinService.class);
        calvinService.stopService(stopServiceIntent);
    }
}
