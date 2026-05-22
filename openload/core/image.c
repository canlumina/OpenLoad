/*
 * OpenLoad - Image 头校验
 */
#include "openload/image.h"
#include "openload/partition.h"
#include "openload/crypto.h"
#include "openload/errno.h"
#include "openload/config.h"
#include <string.h>

int ol_image_read_header(const ol_partition_t *p, ol_image_header_t *hdr_out)
{
    if (!p || !hdr_out) { return OL_E_INVAL; }
    if (p->size < OL_IMAGE_HDR_SIZE) { return OL_E_IMAGE_SIZE; }

    int rc = ol_part_read(p, 0, hdr_out, sizeof(*hdr_out));
    if (rc != OL_OK) { return rc; }

    if (hdr_out->magic != OL_IMAGE_MAGIC) {
        return OL_E_IMAGE_MAGIC;
    }

    /* 头部前 60 字节 CRC, 最后 4 字节是 hdr_crc32 本身 */
    uint32_t calc = ol_crc32(0, hdr_out, OL_IMAGE_HDR_SIZE - sizeof(uint32_t));
    if (calc != hdr_out->hdr_crc32) {
        return OL_E_IMAGE_HDR_CRC;
    }
    return OL_OK;
}

int ol_image_verify(const ol_partition_t *p)
{
    ol_image_header_t hdr;
    int rc = ol_image_read_header(p, &hdr);
    if (rc != OL_OK) { return rc; }

    /* board_id 检查: 0 = 不检查 (跨板通用固件) */
#if OPENLOAD_BOARD_ID
    if (hdr.board_id != 0 && hdr.board_id != OPENLOAD_BOARD_ID) {
        return OL_E_IMAGE_BOARD;
    }
#endif

    if (hdr.firmware_size == 0 ||
        hdr.firmware_size > p->size - OL_IMAGE_HDR_SIZE) {
        return OL_E_IMAGE_SIZE;
    }

    uint32_t calc = ol_crc32_partition(p, OL_IMAGE_HDR_SIZE, hdr.firmware_size);
    if (calc != hdr.firmware_crc32) {
        return OL_E_IMAGE_PAYLOAD_CRC;
    }

    return OL_OK;
}

void ol_image_seal_header(ol_image_header_t *hdr)
{
    if (!hdr) { return; }
    hdr->hdr_crc32 = 0;
    hdr->hdr_crc32 = ol_crc32(0, hdr, OL_IMAGE_HDR_SIZE - sizeof(uint32_t));
}
