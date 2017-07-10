package ericsson.com.calvin.calvin_constrained;

import com.google.gson.Gson;

/**
 * Created by alexander on 2017-07-07.
 */

public class CalvinAttributes {

	public Public indexed_public;

	static class Public {
		static class Owner {
			public String organization, organizationalUnit, role, personOrGroup;
		}

		static class Address {
			public String country, stateOrProvince, locality, street, streetNumber, building, floor, room;
		}

		static class NodeName {
			public String organization, organizationalUnit, purpose, group, name;
		}

		Owner owner;
		Address address;
		NodeName node_name;
	}

	public static CalvinAttributes getAttributes(String name) {
		CalvinAttributes atr = new CalvinAttributes();
		atr.indexed_public = new Public();
		atr.indexed_public.owner = new Public.Owner();
		atr.indexed_public.address = new Public.Address();
		atr.indexed_public.node_name = new Public.NodeName();

		atr.indexed_public.node_name.name = name;
		atr.indexed_public.node_name.group = "Android runtime";
		return atr;
	}

	public String toJson() {
		Gson gson = new Gson();
		return gson.toJson(this, this.getClass());
	}
}