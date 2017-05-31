package io.casey.musikcube.remote.util;

import com.neovisionaries.ws.client.WebSocketFactory;

import java.security.cert.CertificateException;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

import okhttp3.OkHttpClient;

public class NetworkUtil {
    private static SSLContext sslContext;
    private static SSLSocketFactory insecureSslSocketFactory;

    private static SSLSocketFactory originalHttpsUrlConnectionSocketFactory;
    private static HostnameVerifier originalHttpsUrlConnectionHostnameVerifier;

    public synchronized static void init() {
        if (originalHttpsUrlConnectionHostnameVerifier == null) {
            originalHttpsUrlConnectionSocketFactory = HttpsURLConnection.getDefaultSSLSocketFactory();
            originalHttpsUrlConnectionHostnameVerifier = HttpsURLConnection.getDefaultHostnameVerifier();
        }

        if (sslContext == null) {
            try {
                sslContext = SSLContext.getInstance("TLS");
                sslContext.init(null, trustAllCerts, new java.security.SecureRandom());
                insecureSslSocketFactory = sslContext.getSocketFactory();
            }
            catch (Exception ex) {
                throw new RuntimeException(ex);
            }
        }
    }

    public static void disableCertificateValidation(final OkHttpClient.Builder okHttpClient) {
        okHttpClient.sslSocketFactory(insecureSslSocketFactory, (X509TrustManager)trustAllCerts[0]);
        okHttpClient.hostnameVerifier((hostname, session) -> true);
    }

    public static void disableCertificateValidation(final WebSocketFactory socketFactory) {
        socketFactory.setSSLContext(sslContext);
    }

    public static void disableCertificateValidation() {
        try {
            HttpsURLConnection.setDefaultSSLSocketFactory(insecureSslSocketFactory);
            HttpsURLConnection.setDefaultHostnameVerifier((url, session) -> true);
        }
        catch (Exception e) {
            throw new RuntimeException("should never happen");
        }
    }

    public static void enableCertificateValidation() {
        try {
            HttpsURLConnection.setDefaultSSLSocketFactory(originalHttpsUrlConnectionSocketFactory);
            HttpsURLConnection.setDefaultHostnameVerifier(originalHttpsUrlConnectionHostnameVerifier);
        }
        catch (Exception e) {
            throw new RuntimeException("should never happen");
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
