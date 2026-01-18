/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace kcenon::network::testing
{

/**
 * @brief RAII wrapper for SSL_CTX with automatic cleanup
 */
class ssl_context_wrapper
{
public:
	explicit ssl_context_wrapper(SSL_CTX* ctx) : ctx_(ctx) {}

	~ssl_context_wrapper()
	{
		if (ctx_)
		{
			SSL_CTX_free(ctx_);
		}
	}

	// Non-copyable
	ssl_context_wrapper(const ssl_context_wrapper&) = delete;
	ssl_context_wrapper& operator=(const ssl_context_wrapper&) = delete;

	// Movable
	ssl_context_wrapper(ssl_context_wrapper&& other) noexcept
		: ctx_(other.ctx_)
	{
		other.ctx_ = nullptr;
	}

	ssl_context_wrapper& operator=(ssl_context_wrapper&& other) noexcept
	{
		if (this != &other)
		{
			if (ctx_)
			{
				SSL_CTX_free(ctx_);
			}
			ctx_ = other.ctx_;
			other.ctx_ = nullptr;
		}
		return *this;
	}

	SSL_CTX* get() const { return ctx_; }
	explicit operator bool() const { return ctx_ != nullptr; }

private:
	SSL_CTX* ctx_;
};

/**
 * @brief Generates a self-signed test certificate and private key in memory
 *
 * This is used for testing purposes only. The certificate is valid for 1 day
 * and uses RSA 2048-bit key.
 */
class test_certificate_generator
{
public:
	struct certificate_pair
	{
		std::vector<uint8_t> certificate_pem;
		std::vector<uint8_t> private_key_pem;
	};

	/**
	 * @brief Generate a self-signed certificate for testing
	 * @param common_name The CN field for the certificate
	 * @return A pair of PEM-encoded certificate and private key
	 */
	static certificate_pair generate(const std::string& common_name = "localhost")
	{
		certificate_pair result;

		// Generate RSA key pair using OpenSSL 3.x EVP API
		EVP_PKEY* pkey = nullptr;
		EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
		if (!ctx)
		{
			throw std::runtime_error("Failed to create EVP_PKEY_CTX");
		}

		if (EVP_PKEY_keygen_init(ctx) <= 0 ||
		    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ||
		    EVP_PKEY_keygen(ctx, &pkey) <= 0)
		{
			EVP_PKEY_CTX_free(ctx);
			throw std::runtime_error("Failed to generate RSA key");
		}
		EVP_PKEY_CTX_free(ctx);

		// Create X509 certificate
		X509* x509 = X509_new();
		if (!x509)
		{
			EVP_PKEY_free(pkey);
			throw std::runtime_error("Failed to allocate X509");
		}

		// Set certificate version (v3)
		X509_set_version(x509, 2);

		// Set serial number
		ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

		// Set validity period (1 day)
		X509_gmtime_adj(X509_get_notBefore(x509), 0);
		X509_gmtime_adj(X509_get_notAfter(x509), 86400L);

		// Set public key
		X509_set_pubkey(x509, pkey);

		// Set subject name
		X509_NAME* name = X509_get_subject_name(x509);
		X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC,
		                           reinterpret_cast<const unsigned char*>("US"), -1, -1, 0);
		X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
		                           reinterpret_cast<const unsigned char*>("Test Organization"), -1, -1, 0);
		X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
		                           reinterpret_cast<const unsigned char*>(common_name.c_str()), -1, -1, 0);

		// Self-signed: issuer = subject
		X509_set_issuer_name(x509, name);

		// Sign the certificate
		if (!X509_sign(x509, pkey, EVP_sha256()))
		{
			X509_free(x509);
			EVP_PKEY_free(pkey);
			throw std::runtime_error("Failed to sign certificate");
		}

		// Convert certificate to PEM
		BIO* cert_bio = BIO_new(BIO_s_mem());
		PEM_write_bio_X509(cert_bio, x509);
		BUF_MEM* cert_buf;
		BIO_get_mem_ptr(cert_bio, &cert_buf);
		result.certificate_pem.assign(cert_buf->data, cert_buf->data + cert_buf->length);
		BIO_free(cert_bio);

		// Convert private key to PEM
		BIO* key_bio = BIO_new(BIO_s_mem());
		PEM_write_bio_PrivateKey(key_bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
		BUF_MEM* key_buf;
		BIO_get_mem_ptr(key_bio, &key_buf);
		result.private_key_pem.assign(key_buf->data, key_buf->data + key_buf->length);
		BIO_free(key_bio);

		// Cleanup
		X509_free(x509);
		EVP_PKEY_free(pkey);

		return result;
	}
};

/**
 * @brief Creates SSL_CTX objects for DTLS testing
 */
class dtls_context_factory
{
public:
	/**
	 * @brief Create a DTLS server context with test certificates
	 * @param cert_pair The certificate and key pair to use
	 * @return SSL_CTX configured for DTLS server
	 */
	static ssl_context_wrapper create_server_context(
		const test_certificate_generator::certificate_pair& cert_pair)
	{
		const SSL_METHOD* method = DTLS_server_method();
		SSL_CTX* ctx = SSL_CTX_new(method);
		if (!ctx)
		{
			throw std::runtime_error("Failed to create DTLS server context");
		}

		// Load certificate from memory
		BIO* cert_bio = BIO_new_mem_buf(cert_pair.certificate_pem.data(),
		                                 static_cast<int>(cert_pair.certificate_pem.size()));
		X509* cert = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
		BIO_free(cert_bio);

		if (!cert || SSL_CTX_use_certificate(ctx, cert) != 1)
		{
			if (cert) X509_free(cert);
			SSL_CTX_free(ctx);
			throw std::runtime_error("Failed to load certificate");
		}
		X509_free(cert);

		// Load private key from memory
		BIO* key_bio = BIO_new_mem_buf(cert_pair.private_key_pem.data(),
		                                static_cast<int>(cert_pair.private_key_pem.size()));
		EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
		BIO_free(key_bio);

		if (!pkey || SSL_CTX_use_PrivateKey(ctx, pkey) != 1)
		{
			if (pkey) EVP_PKEY_free(pkey);
			SSL_CTX_free(ctx);
			throw std::runtime_error("Failed to load private key");
		}
		EVP_PKEY_free(pkey);

		// Verify key matches certificate
		if (SSL_CTX_check_private_key(ctx) != 1)
		{
			SSL_CTX_free(ctx);
			throw std::runtime_error("Private key does not match certificate");
		}

		return ssl_context_wrapper(ctx);
	}

	/**
	 * @brief Create a DTLS client context
	 * @param verify_peer Whether to verify the server certificate
	 * @return SSL_CTX configured for DTLS client
	 */
	static ssl_context_wrapper create_client_context(bool verify_peer = false)
	{
		const SSL_METHOD* method = DTLS_client_method();
		SSL_CTX* ctx = SSL_CTX_new(method);
		if (!ctx)
		{
			throw std::runtime_error("Failed to create DTLS client context");
		}

		if (verify_peer)
		{
			SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
		}
		else
		{
			SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
		}

		return ssl_context_wrapper(ctx);
	}

	/**
	 * @brief Create a DTLS client context with client certificate
	 * @param cert_pair The certificate and key pair to use
	 * @param verify_peer Whether to verify the server certificate
	 * @return SSL_CTX configured for DTLS client with certificate
	 */
	static ssl_context_wrapper create_client_context_with_cert(
		const test_certificate_generator::certificate_pair& cert_pair,
		bool verify_peer = false)
	{
		auto ctx_wrapper = create_client_context(verify_peer);
		SSL_CTX* ctx = ctx_wrapper.get();

		// Load certificate from memory
		BIO* cert_bio = BIO_new_mem_buf(cert_pair.certificate_pem.data(),
		                                 static_cast<int>(cert_pair.certificate_pem.size()));
		X509* cert = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
		BIO_free(cert_bio);

		if (!cert || SSL_CTX_use_certificate(ctx, cert) != 1)
		{
			if (cert) X509_free(cert);
			throw std::runtime_error("Failed to load client certificate");
		}
		X509_free(cert);

		// Load private key from memory
		BIO* key_bio = BIO_new_mem_buf(cert_pair.private_key_pem.data(),
		                                static_cast<int>(cert_pair.private_key_pem.size()));
		EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
		BIO_free(key_bio);

		if (!pkey || SSL_CTX_use_PrivateKey(ctx, pkey) != 1)
		{
			if (pkey) EVP_PKEY_free(pkey);
			throw std::runtime_error("Failed to load client private key");
		}
		EVP_PKEY_free(pkey);

		return ctx_wrapper;
	}
};

/**
 * @brief Utility to find an available UDP port for testing
 * @param start_port The port to start searching from
 * @return An available port number
 */
inline uint16_t find_available_udp_port(uint16_t start_port = 15000)
{
	for (uint16_t port = start_port; port < 65535; ++port)
	{
		int sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock < 0)
		{
			continue;
		}

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = htons(port);

		int result = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif

		if (result == 0)
		{
			return port;
		}
	}

	throw std::runtime_error("No available UDP port found");
}

} // namespace kcenon::network::testing
