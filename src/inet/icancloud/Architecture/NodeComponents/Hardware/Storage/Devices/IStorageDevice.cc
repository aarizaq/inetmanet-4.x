#include "inet/icancloud/Architecture/NodeComponents/Hardware/Storage/Devices/IStorageDevice.h"

namespace inet {

namespace icancloud {


void IStorageDevice::initialize(int stage){

    HWEnergyInterface::initialize(stage);
    if (stage == INITSTAGE_PHYSICAL_ENVIRONMENT) {
        storageMod = check_and_cast<StorageController*> (getParentModule()->getParentModule()->getSubmodule("eController"));
        if (this->isVector()) {
            storageMod->registerDevice(this, getIndex());
        }
        else
            storageMod->registerDevice(this, getParentModule()->getIndex());
    }
}

void IStorageDevice::e_changeState (const string &energyState){
    if (this->isVector())
        storageMod->e_changeState(energyState, getIndex());
    else
        storageMod->e_changeState(energyState, getParentModule()->getIndex());
}

} // namespace icancloud
} // namespace inet
