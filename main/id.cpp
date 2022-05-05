#include "error.h"
#include "Xprt.h"

namespace hycast {

using String = std::string;

// Protocol data unit (PDU) identifiers
class Id : public XprtAble
{
    uint16_t value;

public:
    using Type = decltype(value);

    static constexpr Id UNSET(0);
    static constexpr Id PROTOCOL_VERSION(1);
    static constexpr Id IS_PUBLISHER(2);
    static constexpr Id PEER_SRVR_ADDRS(3);
    static constexpr Id PUB_PATH_NOTICE(4);
    static constexpr Id PROD_INFO_NOTICE(5);
    static constexpr Id DATA_SEG_NOTICE(6);
    static constexpr Id PROD_INFO_REQUEST(7);
    static constexpr Id DATA_SEG_REQUEST(8);
    static constexpr Id PEER_SRVR_ADDR(9);
    static constexpr Id PROD_INFO(10);
    static constexpr Id DATA_SEG(11);
    //static constexpr Id MAX_PDU_ID(DATA_SEG);

    /**
     * Constructs.
     *
     * @param[in] value            PDU ID value
     * @throws    IllegalArgument  `value` is unsupported
     */
    Id(Type value)
        : value(value)
    {
        if (value > MAX_PDU_ID)
            throw INVALID_ARGUMENT("value=" + to_string());
    }

    Id()
        : value(UNSET)
    {}

    operator bool() const noexcept {
        return value != UNSET;
    }

    inline String to_string() const {
        return std::to_string(value);
    }

    inline operator Type() const noexcept {
        return value;
    }

    inline bool operator==(const Id rhs) const noexcept {
        return value == rhs.value;
    }

    inline bool operator==(const Type rhs) const noexcept {
        return value == rhs;
    }

    inline bool write(Xprt xprt) const {
        return xprt.write(value);
    }

    inline bool read(Xprt xprt) {
        return xprt.read(value);
    }
};

}
