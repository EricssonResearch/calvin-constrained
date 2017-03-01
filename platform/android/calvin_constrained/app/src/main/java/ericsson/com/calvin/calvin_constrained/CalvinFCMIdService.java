package ericsson.com.calvin.calvin_constrained;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.google.firebase.iid.FirebaseInstanceId;
import com.google.firebase.iid.FirebaseInstanceIdService;

/**
 * Created by alexander on 2017-02-07.
 */

public class CalvinFCMIdService extends FirebaseInstanceIdService {

    public static String SP_FCMTOKEN_NAME = "FCM_TOKEN";
    public static String SP_FCMTOKEN_NAME_KEY = "TOKEN";

    private final String LOG_TAG = "GCM ID Service";

    @Override
    public void onTokenRefresh() {
        String refreshedToken = FirebaseInstanceId.getInstance().getToken();
        Log.d(LOG_TAG, "Token refreshed to: " + refreshedToken);

        // Store token in SP
        SharedPreferences sp = getApplication().getSharedPreferences(SP_FCMTOKEN_NAME, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sp.edit();
        editor.putString(SP_FCMTOKEN_NAME, refreshedToken);

        // TODO: Update calvinuri in DHT
    }

    public static String getFCMDeviceToken(Application app){
        SharedPreferences sp = app.getSharedPreferences(SP_FCMTOKEN_NAME, Context.MODE_PRIVATE);
        return sp.getString(SP_FCMTOKEN_NAME_KEY, null);
    }
}
