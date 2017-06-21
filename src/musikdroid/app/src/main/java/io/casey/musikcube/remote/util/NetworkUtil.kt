package io.casey.musikcube.remote.util

import com.neovisionaries.ws.client.WebSocketFactory
import okhttp3.OkHttpClient
import java.security.cert.CertificateException
import javax.net.ssl.*

object NetworkUtil {
    private var sslContext: SSLContext? = null
    private var insecureSslSocketFactory: SSLSocketFactory? = null
    private var originalHttpsUrlConnectionSocketFactory: SSLSocketFactory? = null
    private var originalHttpsUrlConnectionHostnameVerifier: HostnameVerifier? = null

    @Synchronized fun init() {
        if (originalHttpsUrlConnectionHostnameVerifier == null) {
            originalHttpsUrlConnectionSocketFactory = HttpsURLConnection.getDefaultSSLSocketFactory()
            originalHttpsUrlConnectionHostnameVerifier = HttpsURLConnection.getDefaultHostnameVerifier()
        }

        if (sslContext == null) {
            try {
                sslContext = SSLContext.getInstance("TLS")
                sslContext?.init(null, trustAllCerts, java.security.SecureRandom())
                insecureSslSocketFactory = sslContext?.socketFactory
            }
            catch (ex: Exception) {
                throw RuntimeException(ex)
            }
        }
    }

    fun disableCertificateValidation(okHttpClient: OkHttpClient.Builder) {
        okHttpClient.sslSocketFactory(insecureSslSocketFactory!!, trustAllCerts[0] as X509TrustManager)
        okHttpClient.hostnameVerifier { _, _ -> true }
    }

    fun disableCertificateValidation(socketFactory: WebSocketFactory) {
        socketFactory.sslContext = sslContext
    }

    fun disableCertificateValidation() {
        try {
            HttpsURLConnection.setDefaultSSLSocketFactory(insecureSslSocketFactory)
            HttpsURLConnection.setDefaultHostnameVerifier { _, _ -> true }
        }
        catch (e: Exception) {
            throw RuntimeException("should never happen")
        }
    }

    fun enableCertificateValidation() {
        try {
            HttpsURLConnection.setDefaultSSLSocketFactory(originalHttpsUrlConnectionSocketFactory)
            HttpsURLConnection.setDefaultHostnameVerifier(originalHttpsUrlConnectionHostnameVerifier)
        }
        catch (e: Exception) {
            throw RuntimeException("should never happen")
        }
    }

    private val trustAllCerts = arrayOf<TrustManager>(object : X509TrustManager {
        @Throws(CertificateException::class)
        override fun checkClientTrusted(chain: Array<java.security.cert.X509Certificate>, authType: String) {
        }

        @Throws(CertificateException::class)
        override fun checkServerTrusted(chain: Array<java.security.cert.X509Certificate>, authType: String) {
        }

        override fun getAcceptedIssuers(): Array<java.security.cert.X509Certificate> {
            return arrayOf()
        }
    })
}
