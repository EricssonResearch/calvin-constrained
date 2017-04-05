package ericsson.com.calvin.calvin_constrained;

/**
 * Created by alexander on 2017-02-13.
 */

/**
 * Abstract class that implements a handler for a message sent between the runtime and the Java service.
 */
public abstract class CalvinMessageHandler {
    public static final String RUNTIME_STOP = "RS";
    public static final String RUNTIME_CALVIN_MSG = "CM";
    public static final String RUNTIME_STARTED = "RR";
    public static final String FCM_CONNECT = "FC";
    public static final String INIT_EXTERNAL_CALVINSYS = "CI";
    public static final String DESTROY_EXTERNAL_CALVINSYS = "CD";
    public static final String PAYLOAD_EXTERNAL_CALVINSYS = "CP";

    public abstract String getCommand();
    public abstract void handleData(byte[] data);
}