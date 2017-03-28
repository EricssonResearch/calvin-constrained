package ericsson.com.calvin.calvin_constrained;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Bundle;
import android.util.Log;

/**
 * Created by alexander on 2017-03-24.
 */

public class CalvinBroadcastReceiver extends BroadcastReceiver {
	private final String LOG_TAG = "CalvinBR";
	Calvin calvin;

	public CalvinBroadcastReceiver(Calvin calvin) {
		this.calvin = calvin;
	}

	@Override
	public void onReceive(Context context, Intent intent) {
		Log.d(LOG_TAG, "On receive");
		if (this.calvin == null) {
			Log.e(LOG_TAG, "Could not get calvin instance");
			return;
		}
		switch(intent.getAction()) {
			case ConnectivityManager.CONNECTIVITY_ACTION:
				connectivityChanged(context, intent);
				break;
		}
	}

	private void connectivityChanged(Context context, Intent intent) {
		if (intent == null) {
			Log.e(LOG_TAG, "intent was null");
			return;
		}
		Bundle bundle = intent.getExtras();
		NetworkInfo ni = bundle.getParcelable(ConnectivityManager.EXTRA_NETWORK_INFO);
		if (ni != null) {
			if (ni.getState() == NetworkInfo.State.CONNECTED && !isInitialStickyBroadcast()) {
				Log.d(LOG_TAG, "Newly connected network!");
				calvin.sendUpstreamFCMMessages(context); // Send any queued messages during downtime
				calvin.triggerConnectivityChange(calvin.node);
			}
		}
	}
}