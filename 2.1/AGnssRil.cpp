/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "GnssHAL_AGnssRilInterface"

#include "AGnssRil.h"

namespace android {
namespace hardware {
namespace gnss {
namespace V2_0 {
namespace implementation {

std::vector<std::unique_ptr<ThreadFuncArgs>> AGnssRil::sThreadFuncArgsList;
sp<IAGnssRilCallback> AGnssRil::sAGnssRilCbIface = nullptr;
bool AGnssRil::sInterfaceExists = false;

AGpsRilCallbacks AGnssRil::sAGnssRilCb = {
    .request_setid = AGnssRil::requestSetId,
    .request_refloc = AGnssRil::requestRefLoc,
    .create_thread_cb = AGnssRil::createThreadCb
};

sem_t AGnssRil::sSem;

AGnssRil::AGnssRil(const AGpsRilInterface_ext* aGpsRilIface) : mAGnssRilIface(aGpsRilIface) {
    /* Error out if an instance of the interface already exists. */
    LOG_ALWAYS_FATAL_IF(sInterfaceExists);
    sInterfaceExists = true;
    sem_init(&sSem, 0, 1);
}

AGnssRil::~AGnssRil() {
    sThreadFuncArgsList.clear();
    sInterfaceExists = false;
    sem_destroy(&sSem);
}

void AGnssRil::requestSetId(uint32_t flags) {
    sem_wait(&sSem);
    if (sAGnssRilCbIface == nullptr) {
        ALOGE("%s: AGNSSRil Callback Interface configured incorrectly", __func__);
        sem_post(&sSem);
        return;
    }

    auto ret = sAGnssRilCbIface->requestSetIdCb(flags);
    if (!ret.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }
    sem_post(&sSem);
}

void AGnssRil::requestRefLoc(uint32_t /*flags*/) {
    sem_wait(&sSem);
    if (sAGnssRilCbIface == nullptr) {
        ALOGE("%s: AGNSSRil Callback Interface configured incorrectly", __func__);
        sem_post(&sSem);
        return;
    }

    auto ret = sAGnssRilCbIface->requestRefLocCb();
    if (!ret.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }
    sem_post(&sSem);
}

pthread_t AGnssRil::createThreadCb(const char* name, void (*start)(void*), void* arg) {
    return createPthread(name, start, arg, &sThreadFuncArgsList);
}

// Methods from ::android::hardware::gnss::V1_0::IAGnssRil follow.
Return<void> AGnssRil::setCallback(const sp<IAGnssRilCallback>& callback)  {
    sem_wait(&sSem);
    if (mAGnssRilIface == nullptr) {
        ALOGE("%s: AGnssRil interface is unavailable", __func__);
        sem_post(&sSem);
        return Void();
    }

    sAGnssRilCbIface = callback;

    mAGnssRilIface->init(&sAGnssRilCb);
    sem_post(&sSem);
    ALOGD("setCallback done");
    return Void();
}

Return<void> AGnssRil::setRefLocation(const IAGnssRil::AGnssRefLocation& aGnssRefLocation)  {
    if (mAGnssRilIface == nullptr) {
        ALOGE("%s: AGnssRil interface is unavailable", __func__);
        return Void();
    }

    AGpsRefLocation aGnssRefloc;
    aGnssRefloc.type = static_cast<uint16_t>(aGnssRefLocation.type);

    auto& cellID = aGnssRefLocation.cellID;
    aGnssRefloc.u.cellID = {
        .type = static_cast<uint16_t>(cellID.type),
        .mcc = cellID.mcc,
        .mnc = cellID.mnc,
        .lac = cellID.lac,
        .cid = cellID.cid,
        .tac = cellID.tac,
        .pcid = cellID.pcid
    };

    mAGnssRilIface->set_ref_location(&aGnssRefloc, sizeof(aGnssRefloc));
    return Void();
}

Return<bool> AGnssRil::setSetId(IAGnssRil::SetIDType type, const hidl_string& setid)  {
    if (mAGnssRilIface == nullptr) {
        ALOGE("%s: AGnssRil interface is unavailable", __func__);
        return false;
    }

    mAGnssRilIface->set_set_id(static_cast<uint16_t>(type), setid.c_str());
    return true;
}

Return<bool> AGnssRil::updateNetworkState(bool, V1_0::IAGnssRil::NetworkType, bool) {
    // TODO implement
    return bool{};
}
Return<bool> AGnssRil::updateNetworkState_2_0(
    const V2_0::IAGnssRil::NetworkAttributes& attributes) {
    ALOGD("updateNetworkState_2_0 networkAttributes: %s", toString(attributes).c_str());

    if (mAGnssRilIface == nullptr) {
        ALOGE("%s: AGnssRil interface is unavailable", __func__);
        return false;
    }

    mAGnssRilIface->update_network_state_ext(attributes.networkHandle,
            attributes.isConnected,
            attributes.capabilities,
            attributes.apn.c_str());
    return true;
}

Return<bool> AGnssRil::updateNetworkAvailability(bool available, const hidl_string& apn)  {
    if (mAGnssRilIface == nullptr) {
        ALOGE("%s: AGnssRil interface is unavailable", __func__);
        return false;
    }

    mAGnssRilIface->update_network_availability(available, apn.c_str());
    return true;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace gnss
}  // namespace hardware
}  // namespace android
