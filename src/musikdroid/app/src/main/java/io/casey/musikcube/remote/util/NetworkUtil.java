package io.casey.musikcube.remote.util;

import com.neovisionaries.ws.client.WebSocketFactory;

import java.security.cert.CertificateException;

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

import okhttp3.OkHttpClient;

public class NetworkUtil {
    private static SSLContext sslContext;
    private static SSLSocketFactory sslSocketFactory;

    public static void disableCertificateValidation(final OkHttpClient.Builder okHttpClient) {
        init();
        okHttpClient.sslSocketFactory(sslSocketFactory, (X509TrustManager)trustAllCerts[0]);
        okHttpClient.hostnameVerifier((hostname, session) -> true);
    }

    public static void disableCertificateValidation(final WebSocketFactory socketFactory) {
        socketFactory.setSSLContext(sslContext);
    }

    private synchronized static void init() {
        try {
            sslContext = SSLContext.getInstance("TLS");
            sslContext.init(null, trustAllCerts, new java.security.SecureRandom());
            sslSocketFactory = sslContext.getSocketFactory();
        }
        catch (Exception ex) {
            throw new RuntimeException (ex);
        }
    }

    private static final TrustManager[] trustAllCerts = new TrustManager[] {
        new X509TrustManager() {
            @Override
            public void checkClientTrusted(java.security.cert.X509Certificate[] chain, String authType) throws CertificateException {
            }

            @Override
            public void checkServerTrusted(java.security.cert.X509Certificate[] chain, String authType) throws CertificateException {
            }

            @Override
            public java.security.cert.X509Certificate[] getAcceptedIssuers() {
                return new java.security.cert.X509Certificate[]{};
            }
        }
    };

    private NetworkUtil() {

    }
}
