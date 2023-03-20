/**
 * Digital signature module.
 *
 *        File: Ed25519.h
 *  Created on: Mar 8, 2021
 *      Author: Steven R. Emmerson
 */

#ifndef HYCAST_PROTOCOL_ED25519_H
#define HYCAST_PROTOCOL_ED25519_H

#include <openssl/evp.h>
#include <string>
#include <sys/uio.h>

/**
 * Class for digital signing and verifying.
 */
class Ed25519 final
{
protected:
    EVP_PKEY*   pKey;   ///< Public/private key-pair
    EVP_MD_CTX* mdCtx;  ///< Message digest context
    std::string pubKey; ///< Public key

public:
    using Signature = char[64]; ///< Ed25519 signature

    /**
     * Default constructs. This constructor is appropriate for digital signers.
     *
     * @throw std::runtime_error  Failure
     */
    Ed25519();

    /**
     * Constructs from a public-key returned by `getPubKey()`. This constructor
     * is appropriate for verifiers.
     *
     * @param[in] pubKey          Public-key
     * @throw std::runtime_error  Failure
     * @see getPubKey();
     */
    Ed25519(const std::string& pubKey);

    /**
     * Copy constructs.
     * @param[in] other  The other instance
     */
    Ed25519(Ed25519& other) =delete;

    /**
     * Move constructs.
     * @param[in,out] other  The other instance
     */
    Ed25519(Ed25519&& other) =delete;

    ~Ed25519();

    /**
     * Copy assigns from another instance.
     * @param[in] rhs  The other instance
     * @return         A reference to this just-assigned instance
     */
    Ed25519& operator=(const Ed25519& rhs) =delete;

    /**
     * Move assigns from another instance.
     * @param[in,out] rhs  The other instance
     * @return             A reference to this just-assigned instance
     */
    Ed25519& operator=(const Ed25519&& rhs) =delete;

    /**
     * Returns a printable version of the public key in a form suitable for
     * construction by `Ed25519(std::string&)`.
     *
     * @return                    Public key
     * @throw std::runtime_error  Failure
     * @see Ed25519(std::string&)
     */
    std::string getPubKey() const;

    /**
     * Signs a message.
     *
     * @param[in]     msg         Message to be signed
     * @param[in]     msgLen      Length of message in bytes
     * @param[out]    sig         Message signature
     * @return                    Length of signature in bytes
     * @throw std::runtime_error  Failure
     */
    size_t sign(const char*  msg,
                const size_t msgLen,
                Signature    sig);

    /**
     * Signs a message.
     *
     * @param[in]  msg            Message to be signed
     * @return                    Message signature
     * @throw std::runtime_error  Failure
     */
    std::string sign(const std::string& msg);

    /**
     * Verifies a signed message.
     *
     * @param[in]  msg            Message to be verified
     * @param[in]  msgLen         Length of message in bytes
     * @param[in]  sig            Message signature
     * @retval     true           Message is verified
     * @retval     false          Message is not verified
     * @throw std::runtime_error  Failure
     */
    bool verify(const char*     msg,
                const size_t    msgLen,
                const Signature sig);

    /**
     * Verifies a signed message.
     *
     * @param[in]  msg            Message to be verified
     * @param[in]  sig            Message signature
     * @retval     true           Message is verified
     * @retval     false          Message is not verified
     * @throw std::runtime_error  Failure
     */
    bool verify(const std::string& msg,
                const std::string& sig);
};

#endif /* HYCAST_PROTOCOL_ED25519_H */
