//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//	[ GPIO bus factory ]
//
//---------------------------------------------------------------------------

#include <memory>

#include "hal/gpiobus_bananam2p.h"
#include "hal/gpiobus_factory.h"
#include "hal/gpiobus_raspberry.h"
#include "hal/gpiobus_virtual.h"
#include "hal/sbc_version.h"
#include "shared/log.h"

using namespace std;

unique_ptr<BUS> GPIOBUS_Factory::Create(BUS::mode_e mode)
{
    unique_ptr<BUS> return_ptr;

    try {
        // TODO Make the factory a friend of GPIOBUS and make the GPIOBUS constructor private
        // so that clients cannot use it anymore but have to use the factory.
        // Also make Init() private.
        SBC_Version::Init();
        if (SBC_Version::IsBananaPi()) {
            LOGTRACE("Creating GPIOBUS_BananaM2p")
            return_ptr = make_unique<GPIOBUS_BananaM2p>();
        } else if (SBC_Version::IsRaspberryPi()) {
            LOGTRACE("Creating GPIOBUS_Raspberry")
            return_ptr = make_unique<GPIOBUS_Raspberry>();
        } else {
            LOGINFO("Creating Virtual GPIOBUS")
            return_ptr = make_unique<GPIOBUS_Virtual>();
        }
        if (!return_ptr->Init(mode)) {
            return nullptr;
        }
        return_ptr->Reset();
    } catch (const invalid_argument&) {
        LOGERROR("Exception while trying to initialize GPIO bus. Are you running as root?")
        return_ptr = nullptr;
    }

    return return_ptr;
}
