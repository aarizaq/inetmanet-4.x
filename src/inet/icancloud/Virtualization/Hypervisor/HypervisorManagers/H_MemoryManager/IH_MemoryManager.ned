//
// Module interface that defines the behavior of a Memory hypervisor controller.
//
// @author Gabriel Gonz&aacute;lez Casta&ntilde;&eacute
// @date 2012-10-23
//
package inet.icancloud.Virtualization.Hypervisor.HypervisorManagers.H_MemoryManager;

moduleinterface IH_MemoryManager {

	parameters:
		int blockSize_KB @unit(KiB) @mutable;
		int memorySize_MB @unit(MiB) @mutable;
		
		int memoryOverhead_MB;
    gates:
		input   fromVMMemoryI[];
        input   fromVMMemoryO[];
        output  toVMMemoryI[];
        output  toVMMemoryO[];

        input   fromNodeMemoryI;
        input   fromNodeMemoryO;
        output  toNodeMemoryI;
        output  toNodeMemoryO;
}

