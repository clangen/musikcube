package io.casey.musikcube.remote.websocket;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.Locale;
import java.util.concurrent.atomic.AtomicInteger;

public class SocketMessage {
    private static final String TAG = SocketMessage.class.getCanonicalName();

    public enum Type {
        Request("request"),
        Response("response"),
        Broadcast("broadcast");

        private String rawType;

        Type(String rawType) {
            this.rawType = rawType;
        }

        public String getRawType() {
            return rawType;
        }

        public static Type fromString(final String str) {
            if (Request.rawType.equals(str)) {
                return Request;
            }
            else if (Response.rawType.equals(str)) {
                return Response;
            }
            else if (Broadcast.rawType.equals(str)) {
                return Broadcast;
            }

            throw new IllegalArgumentException("str");
        }
    }

    private String name;
    private String id;
    private Type type;
    private JSONObject options;

    public static SocketMessage create(String string) {
        try {
            JSONObject object = new JSONObject(string);
            final String name = object.getString("name");
            final Type type = Type.fromString(object.getString("type"));
            final String id = object.getString("id");
            final JSONObject options = object.optJSONObject("options");
            return new SocketMessage(name, id, type, options);
        }
        catch (Exception ex) {
            Log.e(TAG, ex.toString());
            return null;
        }
    }

    private SocketMessage(String name, String id, Type type, JSONObject options) {
        if (name == null || name.length() == 0 || id == null || id.length() == 0 || type == null) {
            throw new IllegalArgumentException();
        }

        this.name = name;
        this.id = id;
        this.type = type;
        this.options = (options == null) ? new JSONObject() : options;
    }

    public String getName() {
        return name;
    }

    public String getId() {
        return id;
    }

    public Type getType() {
        return type;
    }

    public <T> T getOption(final String key) {
        if (options.has(key)) {
            try {
                return (T) options.get(key);
            }
            catch (JSONException ex) {
                /* swallow */
            }
        }
        return null;
    }

    public String getStringOption(final String key) {
        return getStringOption(key, "");
    }

    public String getStringOption(final String key, final String defaultValue) {
        if (options.has(key)) {
            try {
                return options.getString(key);
            }
            catch (JSONException ex) {
                /* swallow */
            }
        }
        return defaultValue;
    }

    public int getIntOption(final String key) {
        return getIntOption(key, 0);
    }

    public int getIntOption(final String key, final int defaultValue) {
        if (options.has(key)) {
            try {
                return options.getInt(key);
            }
            catch (JSONException ex) {
                /* swallow */
            }
        }
        return defaultValue;
    }


    public double getDoubleOption(final String key) {
        return getDoubleOption(key, 0.0);
    }

    public double getDoubleOption(final String key, final double defaultValue) {
        if (options.has(key)) {
            try {
                return options.getDouble(key);
            }
            catch (JSONException ex) {
                /* swallow */
            }
        }
        return defaultValue;
    }

    public boolean getBooleanOption(final String key) {
        return getBooleanOption(key, false);
    }

    public boolean getBooleanOption(final String key, final boolean defaultValue) {
        if (options.has(key)) {
            try {
                return options.getBoolean(key);
            }
            catch (JSONException ex) {
                /* swallow */
            }
        }
        return defaultValue;
    }

    public JSONObject getJsonObjectOption(final String key) {
        return getJsonObjectOption(key, null);
    }

    public JSONObject getJsonObjectOption(final String key, final JSONObject defaultValue) {
        if (options.has(key)) {
            try {
                return options.getJSONObject(key);
            }
            catch (JSONException ex) {
                /* swallow */
            }
        }
        return defaultValue;
    }

    public JSONArray getJsonArrayOption(final String key) {
        return getJsonArrayOption(key, null);
    }

    public JSONArray getJsonArrayOption(final String key, final JSONArray defaultValue) {
        if (options.has(key)) {
            try {
                return options.getJSONArray(key);
            }
            catch (JSONException ex) {
                /* swallow */
            }
        }
        return defaultValue;
    }

    @Override
    public String toString() {
        try {
            final JSONObject json = new JSONObject();
            json.put("name", name);
            json.put("id", id);
            json.put("type", type.getRawType());
            json.put("options", options);
            return json.toString();
        }
        catch (JSONException ex) {
            throw new RuntimeException("unable to generate output JSON!");
        }
    }

    public Builder buildUpon() {
        try {
            final Builder builder = new Builder();
            builder.name = name;
            builder.type = type;
            builder.id = id;
            builder.options = new JSONObject(options.toString());
            return builder;
        }
        catch (JSONException ex) {
            throw new RuntimeException(ex);
        }
    }

    public static class Builder {
        private static AtomicInteger nextId = new AtomicInteger();

        private String name;
        private Type type;
        private String id;
        private JSONObject options = new JSONObject();

        private Builder() {

        }

        private static String newId() {
            return String.format(Locale.ENGLISH, "musikcube-android-client-%d", nextId.incrementAndGet());
        }

        public static Builder broadcast(String name) {
            final Builder builder = new Builder();
            builder.name = name;
            builder.id = newId();
            builder.type = Type.Response;
            return builder;
        }

        public static Builder respondTo(final SocketMessage message) {
            final Builder builder = new Builder();
            builder.name = message.getName();
            builder.id = message.getId();
            builder.type = Type.Response;
            return builder;
        }

        public static Builder request(final String name) {
            final Builder builder = new Builder();
            builder.name = name;
            builder.id = newId();
            builder.type = Type.Request;
            return builder;
        }

        public static Builder request(final Messages.Request name) {
            final Builder builder = new Builder();
            builder.name = name.toString();
            builder.id = newId();
            builder.type = Type.Request;
            return builder;
        }

        public Builder addOption(final String key, final Object value) {
            try {
                options.put(key, value);
            }
            catch (JSONException ex) {
                throw new RuntimeException("addOption failed??");
            }
            return this;
        }

        public Builder removeOption(final String key) {
            options.remove(key);
            return this;
        }

        public SocketMessage build() {
            return new SocketMessage(name, id, type, options);
        }
    }
}
