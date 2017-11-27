package io.casey.musikcube.remote.ui.shared.util

import com.neovisionaries.ws.client.WebSocketFactory
import okhttp3.OkHttpClient
import java.security.cert.CertificateException
import javax.net.ssl.*

object NetworkUtil {
    private var sslContext: SSLContext
    private var insecureSslSocketFactory: SSLSocketFactory
    private var originalHttpsUrlConnectionSocketFactory = HttpsURLConnection.getDefaultSSLSocketFactory()
    private var originalHttpsUrlConnectionHostnameVerifier = HttpsURLConnection.getDefaultHostnameVerifier()

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

    init {
        originalHttpsUrlConnectionSocketFactory
        originalHttpsUrlConnectionHostnameVerifier
        sslContext = SSLContext.getInstance("TLS")
        sslContext.init(null, trustAllCerts, java.security.SecureRandom())
        insecureSslSocketFactory = sslContext.socketFactory
    }

    fun disableCertificateValidation(okHttpClient: OkHttpClient.Builder) {
        okHttpClient.sslSocketFactory(insecureSslSocketFactory, trustAllCerts[0] as X509TrustManager)
        okHttpClient.hostnameVerifier { _, _ -> true }
    }

    fun disableCertificateValidation(socketFactory: WebSocketFactory) {
        socketFactory.sslContext = sslContext
    }

    fun disableCertificateValidation() {
        HttpsURLConnection.setDefaultSSLSocketFactory(insecureSslSocketFactory)
        HttpsURLConnection.setDefaultHostnameVerifier { _, _ -> true }
    }

    fun enableCertificateValidation() {
        HttpsURLConnection.setDefaultSSLSocketFactory(originalHttpsUrlConnectionSocketFactory)
        HttpsURLConnection.setDefaultHostnameVerifier(originalHttpsUrlConnectionHostnameVerifier)
    }
}
