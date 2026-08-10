/*
 * OpenThread table snapshots (router/child/joiner) serialized into byte buffers.
 *
 * Caller must hold the OpenThread lock when calling these helpers.
 */

#include <string.h>

#include "openthread/commissioner.h"
#include "openthread/thread.h"
#include "openthread/thread_ftd.h"

#include "openthread/ot_table_snapshot.h"

bool ot_table_snapshot_build_router_table(otInstance *instance, uint8_t *buf, size_t buf_size, size_t *out_len)
{
    if (!instance || !buf || buf_size < 1 || !out_len) {
        return false;
    }
    uint8_t *p = buf + 1;
    uint8_t count = 0;

    for (uint8_t router_id = 0; router_id <= 62; router_id++) {
        otRouterInfo router_info;
        if (otThreadGetRouterInfo(instance, router_id, &router_info) == OT_ERROR_NONE && router_info.mAllocated) {
            if ((size_t)(p - buf) + 15 > buf_size) {
                break;
            }
            *p++ = router_info.mRouterId;
            *p++ = (uint8_t)(router_info.mRloc16 >> 8);
            *p++ = (uint8_t)(router_info.mRloc16 & 0xFF);
            memcpy(p, router_info.mExtAddress.m8, 8);
            p += 8;
            *p++ = router_info.mLinkQualityIn;
            *p++ = router_info.mLinkQualityOut;
            *p++ = (uint8_t)(router_info.mAge >> 8);
            *p++ = (uint8_t)(router_info.mAge & 0xFF);
            count++;
        }
    }

    buf[0] = count;
    *out_len = (size_t)(p - buf);
    return true;
}

bool ot_table_snapshot_build_child_table(otInstance *instance, uint8_t *buf, size_t buf_size, size_t *out_len)
{
    if (!instance || !buf || buf_size < 1 || !out_len) {
        return false;
    }
    uint8_t *p = buf + 1;
    uint8_t count = 0;

    uint16_t max_children = otThreadGetMaxAllowedChildren(instance);
    for (uint16_t index = 0; index < max_children; index++) {
        otChildInfo child_info;
        if (otThreadGetChildInfoByIndex(instance, index, &child_info) == OT_ERROR_NONE) {
            if ((size_t)(p - buf) + 17 > buf_size) {
                break;
            }
            *p++ = child_info.mChildId;
            *p++ = (uint8_t)(child_info.mRloc16 >> 8);
            *p++ = (uint8_t)(child_info.mRloc16 & 0xFF);
            memcpy(p, child_info.mExtAddress.m8, 8);
            p += 8;
            *p++ = child_info.mLinkQualityIn;
            *p++ = (uint8_t)child_info.mAverageRssi;
            *p++ = child_info.mFullThreadDevice ? 1 : 0;
            *p++ = child_info.mRxOnWhenIdle ? 1 : 0;
            *p++ = (uint8_t)(child_info.mAge >> 8);
            *p++ = (uint8_t)(child_info.mAge & 0xFF);
            count++;
        }
    }

    buf[0] = count;
    *out_len = (size_t)(p - buf);
    return true;
}

bool ot_table_snapshot_build_joiner_table(otInstance *instance, uint8_t *buf, size_t buf_size, size_t *out_len)
{
    if (!instance || !buf || buf_size < 1 || !out_len) {
        return false;
    }
    uint8_t *p = buf + 1;
    uint8_t count = 0;

    uint16_t iterator = 0;
    otJoinerInfo joiner_info;
    while (otCommissionerGetNextJoinerInfo(instance, &iterator, &joiner_info) == OT_ERROR_NONE) {
        /* Worst-case entry is capped by the frame protocol implementation (~50 bytes). */
        if ((size_t)(p - buf) + 50 > buf_size) {
            break;
        }
        *p++ = (uint8_t)joiner_info.mType;
        if (joiner_info.mType == OT_JOINER_INFO_TYPE_EUI64) {
            memcpy(p, joiner_info.mSharedId.mEui64.m8, 8);
            p += 8;
        } else if (joiner_info.mType == OT_JOINER_INFO_TYPE_DISCERNER) {
            uint64_t discerner_val = joiner_info.mSharedId.mDiscerner.mValue;
            uint8_t discerner_len = joiner_info.mSharedId.mDiscerner.mLength;
            *p++ = discerner_len;
            uint8_t discerner_bytes = (discerner_len + 7) / 8;
            for (int i = (int)discerner_bytes - 1; i >= 0; i--) {
                *p++ = (uint8_t)((discerner_val >> (i * 8)) & 0xFF);
            }
        } else {
            memset(p, 0, 8);
            p += 8;
        }

        size_t pskd_len = strlen((const char *)joiner_info.mPskd.m8);
        if (pskd_len > 32) {
            pskd_len = 32;
        }
        *p++ = (uint8_t)pskd_len;
        memcpy(p, joiner_info.mPskd.m8, pskd_len);
        p += pskd_len;

        *p++ = (uint8_t)(joiner_info.mExpirationTime >> 24);
        *p++ = (uint8_t)(joiner_info.mExpirationTime >> 16);
        *p++ = (uint8_t)(joiner_info.mExpirationTime >> 8);
        *p++ = (uint8_t)(joiner_info.mExpirationTime & 0xFF);
        count++;
    }

    buf[0] = count;
    *out_len = (size_t)(p - buf);
    return true;
}
