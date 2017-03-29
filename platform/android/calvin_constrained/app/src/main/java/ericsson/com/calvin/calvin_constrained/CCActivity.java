package ericsson.com.calvin.calvin_constrained;

import android.app.Activity;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
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

    Button stopButton, startButton;
    Activity activity;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.cc_layout);
        this.activity = this;
        Intent startServiceIntent = new Intent(this, CalvinService.class);
        Bundle intentData = new Bundle();
        intentData.putBoolean(CalvinService.CLEAR_SERIALIZATION_FILE, true);
        startServiceIntent.putExtras(intentData);
        startService(startServiceIntent);
        setupUI();
        // showLogs();
        Log.d(LOG_TAG, "FCM token: " + FirebaseInstanceId.getInstance().getToken());
    }

    public void setupUI() {
        stopButton = (Button) findViewById(R.id.stop_rt);
        startButton = (Button) findViewById(R.id.start_rt);

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
    }

    public void showLogs(){
        new PrintLogs().execute();
    }

    private class PrintLogs extends AsyncTask<String, String, String> {
        @Override
        protected String doInBackground(String... params) {
            try{
                Process logcat = Runtime.getRuntime().exec("logcat");
                BufferedReader br = new BufferedReader(new InputStreamReader(logcat.getInputStream()));
                String line;
                while((line = br.readLine()) != null) {
                    publishProgress(line);
                }

            } catch (IOException e) {
                e.printStackTrace();
            }
            return null;
        }

        @Override
        protected void onProgressUpdate(String... values){
            TextView logView = (TextView) findViewById(R.id.logcat);
            ScrollView scroll = (ScrollView) findViewById(R.id.logcat_scroll);
            logView.setText(logView.getText().toString() + "\n" + values[0]);
            scroll.fullScroll(View.FOCUS_DOWN);
        }
    }
}