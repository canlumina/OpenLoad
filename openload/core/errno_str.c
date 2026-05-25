/*
 * OpenLoad - 错误码字符串映射
 */
#include "openload/errno.h"

const char *ol_strerror(int e)
{
    switch (e) {
    case OL_OK:                     return "ok";
    case OL_E_INVAL:                return "invalid arg";
    case OL_E_TIMEOUT:              return "timeout";
    case OL_E_NOMEM:                return "no memory";
    case OL_E_NOT_FOUND:            return "not found";
    case OL_E_BUSY:                 return "busy";
    case OL_E_IO:                   return "io error";
    case OL_E_NOT_SUPPORTED:        return "not supported";

    case OL_E_PART_NOT_FOUND:       return "partition not found";
    case OL_E_PART_OUT_OF_RANGE:    return "partition range";
    case OL_E_PART_WRITE_DENIED:    return "partition write denied";
    case OL_E_PART_ALIGN:           return "partition align";
    case OL_E_PART_NO_DEVICE:       return "partition device missing";

    case OL_E_IMAGE_MAGIC:          return "image magic";
    case OL_E_IMAGE_HDR_CRC:        return "image header crc";
    case OL_E_IMAGE_PAYLOAD_CRC:    return "image payload crc";
    case OL_E_IMAGE_SIZE:           return "image size";
    case OL_E_IMAGE_BOARD:          return "image board id";
    case OL_E_IMAGE_VERSION:        return "image version (rollback)";
    case OL_E_IMAGE_SIGNATURE:      return "image signature";
    case OL_E_IMAGE_HASH:           return "image sha256";

    case OL_E_RX_CANCELED:          return "receiver canceled";
    case OL_E_RX_PROTOCOL:          return "receiver protocol";
    case OL_E_RX_CRC:               return "receiver crc";
    case OL_E_RX_OVERFLOW:          return "receiver overflow";

    case OL_E_CRYPTO_KEY:           return "crypto key";
    case OL_E_CRYPTO_DECRYPT:       return "crypto decrypt";

    case OL_E_AT_ERROR:             return "AT error/fail";
    case OL_E_NET_NO_LINK:          return "network no link";
    case OL_E_NET_HTTP:             return "http status";

    default:                        return "unknown";
    }
}
