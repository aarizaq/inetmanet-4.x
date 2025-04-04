//
// Copyright (C) 2013 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_LIFECYCLECONTROLLER_H
#define __INET_LIFECYCLECONTROLLER_H

#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/common/scenario/IScriptable.h"

namespace inet {

/**
 * Manages operations like shutdown/restart, suspend/resume, crash/recover
 * and similar operations for nodes (routers, hosts, etc), interfaces, and
 * protocols.
 *
 * Overview and usage are described in the NED file, you are advised to read
 * that first. The rest of this documentation concentrates on the C++ API.
 *
 * Operations are represented by C++ class derived from LifecycleOperation.
 * Simple modules that wish to participate in an operation need to implement
 * the ILifecycle interface (C++ class).
 *
 * An operation is initiated by calling the <tt>initiateStateChange(cModule *module,
 * LifecycleOperation *operation)</tt> method of this class. (This is often done
 * from a ScenarioManager script). This method applies the operation to the
 * given module (usually a host, router or network interface compound module).
 *
 * Operations may have multiple stages (think multi-stage initialization),
 * where each stage may take nonzero simulation time. The number of stages
 * are defined by the operation (its getNumStages() method).
 * Within a stage, the submodule tree is traversed, and initiateStateChange()
 * is invoked on each module that implements ILifecycle.
 *
 * Operations may take nonzero simulation time. A module that needs nonzero
 * simulation time to complete a stage (e.g. it wants to close TCP connections
 * or model finite shutdown/reboot time) can signal that in the return value
 * of initiateStateChange(). When it is done, it can signal that to
 * LifecycleController by invoking the callback passed to it in
 * initiateStateChange(). LifecycleController only regards the stage as completed
 * (and goes on to the next stage) when all participating modules have indicated
 * that they are done.
 *
 * Operations can be nested, that is, it's possible to initiate another operation
 * while one is underway.
 *
 * @see ILifecycle, LifecycleOperation
 */
class INET_API LifecycleController
{
  protected:
    class INET_API Callback : public IDoneCallback {
      public:
        LifecycleController *controller = nullptr;
        LifecycleOperation *operation = nullptr;
        cModule *module = nullptr;

        void init(LifecycleController *controller, LifecycleOperation *operation, cModule *module);
        virtual void invoke() override;
    };

    Callback *spareCallback = nullptr;
    std::vector<LifecycleOperation *> runningOperations;

  protected:
    virtual bool resumeOperation(LifecycleOperation *operation);
    virtual void doOneStage(LifecycleOperation *operation, cModule *submodule);
    virtual void moduleOperationStageCompleted(Callback *callback); // invoked from the callback

  public:
    LifecycleController() : spareCallback(nullptr) {}
    virtual ~LifecycleController();

    /**
     * Initiate an operation. See the class documentation and ILifecycle for
     * details. The target module will be taken from the operation object.
     *
     * The return value indicates whether the operation has been completed
     * inside the call (true), or is pending because it will take several
     * events and likely nonzero simulation time to complete (false).
     * In the latter case, and if you provided a completionCallback as
     * parameter, you will be notified via the callback when the operation
     * completes.
     */
    virtual bool initiateOperation(LifecycleOperation *operation, IDoneCallback *completionCallback = nullptr);
};

} // namespace inet

#endif

