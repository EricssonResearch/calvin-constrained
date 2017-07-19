package ericsson.com.calvin.calvin_constrained;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Base64;
import android.util.Log;

import org.msgpack.core.MessagePack;
import org.msgpack.core.MessageUnpacker;
import org.msgpack.value.MapValue;
import org.msgpack.value.Value;
import org.msgpack.value.ValueType;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

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

    public static Map<String, Value> msgpackDecodeMap(byte[] data) {
        HashMap<String, Value> map = new HashMap<String, Value>();
        try {
            MessageUnpacker unpacker = MessagePack.newDefaultUnpacker(data);
            MapValue mv = (MapValue) unpacker.unpackValue();
            Set<Map.Entry<Value, Value>> entries = mv.entrySet();
            for(Map.Entry<Value, Value> entry : entries) {
                if(entry.getKey().getValueType() == ValueType.STRING) {
                    map.put(entry.getKey().asStringValue().asString(), entry.getValue());
                }
            }
        }catch (IOException e){}
        if (!map.isEmpty())
            return map;
        Log.e(LOG_TAG, "Could not find any values in map");
        return null;
    }
}
