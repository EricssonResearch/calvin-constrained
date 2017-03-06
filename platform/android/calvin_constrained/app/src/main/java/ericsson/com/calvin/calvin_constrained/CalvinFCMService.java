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
        Log.d(LOG_TAG, "Received FCM msg from: " + message.getFrom());
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
                calvin.runtimeCalvinPayload(bytes);
                break;
            case "set_connect":
                Log.d(LOG_TAG, "Got connected message");
                calvin.fcmTransportConnected();
                break;
        }
    }
}
