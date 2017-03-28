package ericsson.com.calvin.calvin_constrained;

import android.util.Base64;

/**
 * Created by alexander on 2017-02-16.
 */

/**
 * Common static methods used in Calvin.
 */
public class CalvinCommon {

    public static byte[] base64Encode(byte[] input) {
        return Base64.encode(input, Base64.DEFAULT); // Force base64 encoding to RFC3548 standard
    }

    public static byte[] base64Decode(byte[] input) {
        return Base64.decode(input, Base64.DEFAULT);
    }
}
