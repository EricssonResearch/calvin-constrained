package ericsson.com.calvin.calvin_constrained;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Base64;
import android.util.Log;

/**
 * Created by alexander on 2017-02-16.
 */

/**
 * Common static methods used in Calvin.
 */
public class CalvinCommon {

    public static final String LOG_TAG = "Calvin common";

    public static byte[] base64Encode(byte[] input) {
        return Base64.encode(input, Base64.DEFAULT); // Force base64 encoding to RFC3548 standard
    }

    public static byte[] base64Decode(byte[] input) {
        return Base64.decode(input, Base64.DEFAULT);
    }


    public static void startService(SharedPreferences sp, Context context) {
        Log.d(LOG_TAG, "starting service");
        Intent startServiceIntent = new Intent(context, CalvinService.class);
        Bundle intentData = new Bundle();

        if (sp.getBoolean("clean_serialization", false))
            intentData.putBoolean(CalvinService.CLEAR_SERIALIZATION_FILE, true);
        intentData.putString("rt_uris", sp.getString("rt_uris", ""));
        intentData.putString("rt_name", sp.getString("rt_name", ""));

        startServiceIntent.putExtras(intentData);
        context.startService(startServiceIntent);
    }

    public static void stopService(Context context) {
        Log.d(LOG_TAG, "stopping service");
        Intent stopServiceIntent = new Intent(context, CalvinService.class);
        context.stopService(stopServiceIntent);
    }
}
