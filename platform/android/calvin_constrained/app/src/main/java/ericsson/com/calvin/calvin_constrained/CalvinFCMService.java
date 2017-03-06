package ericsson.com.calvin.calvin_constrained;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.support.annotation.Nullable;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;
import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

/**
 * Created by alexander on 2017-02-07.
 */

public class CalvinFCMService extends FirebaseMessagingService {

    private final String LOG_TAG = "Calvin FCM Service";

    @Override
    public void onSendError(String msgid, Exception e) {
        Log.d(LOG_TAG, "FCM Send error: " + e.toString());
    }

    @Override
    public void onMessageSent(String msgid) {
        Log.d(LOG_TAG, "FCM Sent callback: " + msgid);
    }

    @Override
    public void onMessageReceived(RemoteMessage message) {
        Log.d(LOG_TAG, "Received FCM msg from: a" + message.getFrom());
        Log.d(LOG_TAG, "Received FCM msg payload: " + message.getData());

        Calvin calvin = CalvinService.calvin;
        if (calvin == null) {
            Log.e(LOG_TAG, "Calvin instance was not null.");
            return;
        }

        String msgType = message.getData().get("msg_type");

        switch(msgType){
            case "payload":
                byte[] bytes = message.getData().get("payload").getBytes();
                bytes = CalvinCommon.base64Decode(bytes);
                Log.d(LOG_TAG, "FCM Receive service: payload size: " + bytes.length);
                Log.d(LOG_TAG, "Decoded payload: " + new String(bytes));
                Log.d(LOG_TAG, "Ten first bytes are: ");
                for(int i = 0; i < 25; i++)
                    Log.d(LOG_TAG, "B: " + ((int) bytes[i] & 0xff));

                calvin.runtimeCalvinPayload(bytes);
                break;
            case "set_connect":
                Log.d(LOG_TAG, "Got connected message");
                calvin.fcmTransportConnected();
                break;
        }
    }
}
