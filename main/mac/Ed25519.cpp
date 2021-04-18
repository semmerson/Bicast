/**
 * Digital signature module based on Ed25519
 *
 * @file      Ed25519.cpp
 * @author    Steven R. Emmerson <emmerson@ucar.edu>
 * @version   1.0
 * @date      Mar 29, 2021
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include "Ed25519.h"
#include "SslHelp.h"

#include <cassert>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <stdexcept>

/**
 * Twisted Edwards curve digital signing algorithm
 *   - Relatively fast;
 *   - 128 bit security level;
 *   - Fixed-length, 64-byte signature.
 */
Ed25519::Ed25519()
    : pKey{nullptr}
    , mdCtx{EVP_MD_CTX_new()}
    , pubKey{}
{
    if (mdCtx == nullptr)
        SslHelp::throwOpenSslError("EVP_MD_CTX_new() failure");

    EVP_PKEY_CTX* pKeyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (pKeyCtx == nullptr)
        SslHelp::throwOpenSslError("EVP_PKEY_CTX_new_id() failure");

    try {
        if (EVP_PKEY_keygen_init(pKeyCtx) != 1)
            SslHelp::throwOpenSslError("EVP_PKEY_keygen_init() failure");

        if (EVP_PKEY_keygen(pKeyCtx, &pKey) != 1)
            SslHelp::throwOpenSslError("EVP_PKEY_keygen() failure");

        EVP_PKEY_CTX_free(pKeyCtx);

        BIO* bio = ::BIO_new(BIO_s_mem());
        if (bio == nullptr)
            SslHelp::throwOpenSslError("BIO_new() failure");

        try {
            /*
             * Apparently, all the following steps are necessary to obtain a
             * std::string with a printable public key.
             */
            if (::PEM_write_bio_PUBKEY(bio, pKey) == 0) // Doesn't NUL-terminate
                SslHelp::throwOpenSslError("PEM_write_bio_PUBKEY() failure");

            const size_t keyLen = BIO_pending(bio);
            char         keyBuf[keyLen]; // NB: No trailing NUL

            if (::BIO_read(bio, keyBuf, keyLen) != keyLen)
                SslHelp::throwOpenSslError("BIO_read() failure");

            ::BIO_free_all(bio);

            // Finally!
            pubKey = std::string(keyBuf, keyLen);
        } // `bio` allocated
        catch (const std::exception& ex) {
            ::BIO_free_all(bio);
            throw;
        }
    } // `pKeyCtx` allocated
    catch (const std::exception& ex) {
        EVP_PKEY_CTX_free(pKeyCtx);
        throw;
    }
}

Ed25519::Ed25519(const std::string& pubKey)
    : pKey{nullptr}
    , mdCtx{EVP_MD_CTX_new()}
    , pubKey{}
{
    BIO* bio = BIO_new_mem_buf(pubKey.data(), pubKey.size());
    if (bio == nullptr)
        SslHelp::throwOpenSslError("BIO_new_mem_buf() failure");

    try {
        PEM_read_bio_PUBKEY(bio, &pKey, nullptr, nullptr);
        if (pKey == nullptr)
            SslHelp::throwOpenSslError("PEM_read_bio_PUBKEY() failure");

        BIO_free_all(bio);

        this->pubKey = pubKey;
    } // `bio` allocated
    catch (const std::exception& ex) {
        BIO_free_all(bio);
        throw;
    }
}

Ed25519::~Ed25519()
{
    EVP_PKEY_free(pKey);
    EVP_MD_CTX_free(mdCtx);
}

std::string Ed25519::getPubKey() const
{
    return pubKey;
}

size_t Ed25519::sign(const char*  msg,
                     const size_t msgLen,
                     Signature    sig)
{
    if (!EVP_DigestSignInit(mdCtx, nullptr, nullptr, nullptr, pKey))
         SslHelp::throwOpenSslError("EVP_DigestSignInit() failure");

    size_t len = sizeof(sig);
    if (!EVP_DigestSign(mdCtx,
            reinterpret_cast<unsigned char*>(sig), &len,
            reinterpret_cast<const unsigned char*>(msg), msgLen))
         SslHelp::throwOpenSslError("EVP_DigestSign() failure");

    return len;
}

std::string Ed25519::sign(const std::string& msg)
{
    Signature sig;
    sign(msg.data(), msg.size(), sig);
    return std::string(sig, sizeof(sig));
}

bool Ed25519::verify(const char*     msg,
                     const size_t    msgLen,
                     const Signature sig)
{
    if (!EVP_DigestVerifyInit(mdCtx, nullptr, nullptr, nullptr, pKey))
        SslHelp::throwOpenSslError("EVP_DigestVerifyInit() failure");

    return EVP_DigestVerify(mdCtx,
            reinterpret_cast<const unsigned char*>(sig), sizeof(sig),
            reinterpret_cast<const unsigned char*>(msg), msgLen) == 1;
}

bool Ed25519::verify(const std::string& msg,
                     const std::string& sig)
{
    if (sig.size() < sizeof(Signature))
        throw std::invalid_argument("Signature is too small: " +
                std::to_string(sig.size()) + " bytes");
    return verify(msg.data(), msg.size(), sig.data());
}
