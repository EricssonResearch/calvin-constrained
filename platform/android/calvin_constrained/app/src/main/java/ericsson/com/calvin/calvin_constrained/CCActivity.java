package ericsson.com.calvin.calvin_constrained;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Color;
import android.net.ConnectivityManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import com.google.firebase.iid.FirebaseInstanceId;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 * Dummy activity used for debugging.
 */
public class CCActivity extends Activity {

    private final String LOG_TAG = "CCActivity log";

    Button stopButton, startButton, bindButton;
    Activity activity;
    Context context;
    String buttonSysName = "calvinsys.io.button";
    int colors[] = {Color.BLUE, Color.RED, Color.GREEN, Color.GRAY};
    int colorCounter = 0;
    Messenger service;
    Messenger incommingMessenger = new Messenger(new IncommingMessageHandler());

    Button calvinButton;

    private View.OnClickListener clickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            Message msg = Message.obtain(null, 3, 0, 0, 0);
            Bundle bundle = new Bundle();
            bundle.putString("calvinsys", buttonSysName);
            bundle.putString("command", "trigger");
            bundle.putByteArray("payload", "hej".getBytes()); // Dummy data to button, this should be msgpack
            msg.setData(bundle);
            try {
                service.send(msg);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    };

    private class IncommingMessageHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            Log.d(LOG_TAG, "Button, incomming message!");
            switch(msg.what) {
                case 4:
                    // ACK
                    Log.d(LOG_TAG, "External calvinsys got ack!");
                    break;
                case 5:
                    // Init call
                    calvinButton = new Button(activity);
                    calvinButton.setText("I am a calvin button");
                    calvinButton.setOnClickListener(clickListener);
                    Log.d(LOG_TAG, "Init method of external calvinsys called! (actor would like to use button)");
                    LinearLayout root = (LinearLayout) findViewById(R.id.activity_hello_jni);
                    root.addView(calvinButton, 0);
                    break;
                case 3:
                    // Payload call
                    Bundle bundle = msg.getData();
                    Log.d(LOG_TAG, "Calvinsys button callback cmd: " + bundle.getString("command") + ", payload: " + bundle.getString("payload"));
                    calvinButton.setBackgroundColor(colors[colorCounter]);
                    colorCounter = (++colorCounter) % colors.length;
                    break;
            }
        }
    }

    private ServiceConnection connection = new ServiceConnection() {

        @Override
        public void onServiceConnected(ComponentName name, IBinder s) {
            service = new Messenger(s);
            Message pack = Message.obtain(null, 1, 0, 0);
            Bundle bundle = new Bundle();
            bundle.putString("name", buttonSysName);
            pack.setData(bundle);
            pack.replyTo = incommingMessenger;
            try {
                service.send(pack);
                Log.d(LOG_TAG, "Send data");
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            service = null;
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.cc_layout);
        context = this;
        this.activity = this;
        Intent startServiceIntent = new Intent(this, CalvinService.class);
        Bundle intentData = new Bundle();
        intentData.putBoolean(CalvinService.CLEAR_SERIALIZATION_FILE, true);
        startServiceIntent.putExtras(intentData);
        startService(startServiceIntent);
        // showLogs();
        Log.d(LOG_TAG, "FCM token: " + FirebaseInstanceId.getInstance().getToken());
    }

    @Override
    public void onPause() {
        super.onPause();
        //this.cc.onDestroy();
    }

    @Override
    public void onStart() {
        super.onStart();
    }


    @Override
    public void onResume() {
        super.onResume();
        setupUI();
    }

    public void setupUI() {
        stopButton = (Button) findViewById(R.id.stop_rt);
        startButton = (Button) findViewById(R.id.start_rt);
        bindButton = (Button) findViewById(R.id.bind);
        LinearLayout root = (LinearLayout) findViewById(R.id.activity_hello_jni);

        stopButton.setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View v) {
                Intent stopServiceIntent = new Intent(activity, CalvinService.class);
                stopService(stopServiceIntent);
            }
        });
        startButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent startServiceIntent = new Intent(activity, CalvinService.class);
                startService(startServiceIntent);
            }
        });
        bindButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(context, CalvinService.class);
                bindService(intent, connection, Context.BIND_AUTO_CREATE);
            }
        });
    }
}