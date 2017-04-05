package ericsson.com.calvin.calvin_constrained;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.EditTextPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.util.Log;

/**
 * Created by alexander on 2017-04-04.
 */

public class CalvinPreferenceFragment extends PreferenceFragment {

	private Preference.OnPreferenceChangeListener sumSet = new Preference.OnPreferenceChangeListener() {
		@Override
		public boolean onPreferenceChange(Preference preference, Object newValue) {
			if (preference instanceof EditTextPreference)
				preference.setSummary((String) newValue);
			return true;
		}
	};

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.xml.preference);
		Preference start_rt, stop_rt, etp, uris;

		start_rt = findPreference("rt_start");
		stop_rt = findPreference("rt_stop");
		etp = findPreference("rt_name");
		uris = findPreference("rt_uris");

		final SharedPreferences sp = getPreferenceScreen().getSharedPreferences();
		etp.setSummary(sp.getString("rt_name", ""));
		uris.setSummary(sp.getString("rt_uris", ""));
		etp.setOnPreferenceChangeListener(sumSet);
		uris.setOnPreferenceChangeListener(sumSet);
		start_rt.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference preference) {
				CalvinCommon.startService(sp, getActivity());
				return true;
			}
		});
		stop_rt.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference preference) {
				CalvinCommon.stopService(getActivity());
				return true;
			}
		});
	}
}
