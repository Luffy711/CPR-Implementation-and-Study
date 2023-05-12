#include <assert.h>
#include<iostream>
#include<renamer.h>

using namespace std;

renamer::renamer(uint64_t n_log_regs, uint64_t n_phys_regs, uint64_t n_branches, uint64_t n_active)
{
    assert(n_phys_regs > n_log_regs);
    assert((n_branches >= 1) && (n_branches <= 64));
    assert(n_active > 0);

    numLogReg = n_log_regs;
    numPhyReg = n_phys_regs;
    numMaxBranch = n_branches;
    numActiveInst = n_active;

    RMT = new rmt_amt_t[numLogReg];
    AMT = new rmt_amt_t[numLogReg];

    FreeList.Entry = new freelist_entries_t[numPhyReg - numLogReg];
    ActiveList.Entry = new actlist_entries_t[numActiveInst];
    PRF = new prf_t[numPhyReg];
    PRF_RdybitArr = new prf_rdybit_array_t[numPhyReg];
    CPRChkpts = new cpr_chkpts_t[numMaxBranch];
    uint64_t i;
    for (i=0; i<(numMaxBranch); i++)
    {
        CPRChkpts[i].SMT = new rmt_amt_t[numLogReg];
        CPRChkpts[i].unmapped_bits = new bool[numPhyReg];
    }

    // Chkpt_ID = 0;
    for (i=0; i<numLogReg; i++)
    {
        RMT[i].PhyRegMap = i;
        AMT[i].PhyRegMap = i;
    }
    for (i=numLogReg; i<numPhyReg; i++)
    {
        FreeList.Entry[i-numLogReg].PhyRegNum = i;
    }
    FreeList.head.phasebit = 0;
    FreeList.tail.phasebit = 1;
    FreeList.head.value = 0;
    FreeList.tail.value = 0;
    
    for (i=0; i<(numActiveInst); i++)
    {
        ActiveList.Entry[i].compbit = 0;
        ActiveList.Entry[i].excptbit = 0;
        ActiveList.Entry[i].ldviobit = 0;
        ActiveList.Entry[i].brmispbit = 0;
        ActiveList.Entry[i].valmispbit = 0;
        ActiveList.Entry[i].ldflag = 0;
        ActiveList.Entry[i].strflag = 0;
        ActiveList.Entry[i].brflag = 0;
        ActiveList.Entry[i].amoflag = 0;
        ActiveList.Entry[i].csrflag = 0;
    }
    ActiveList.head.phasebit = 0;
    ActiveList.tail.phasebit = 0;
    ActiveList.head.value = 0;
    ActiveList.tail.value = 0;

    for (i=0; i<(numPhyReg); i++)
    {
		PRF[i].value = 0;
		PRF_RdybitArr[i].readybit = 1;
		if (i < numLogReg)
		{
			PRF[i].unmapped_bits = 0;	
			PRF[i].usage_counter = 1;		
		}
		else
		{
			PRF[i].unmapped_bits = 1;
			PRF[i].usage_counter = 0;
		}
    }
    
	
	for (uint64_t j=0; j<(numLogReg); j++)
	{
		CPRChkpts[0].SMT[j].PhyRegMap = RMT[j].PhyRegMap;
	}
    for (uint64_t j=0; j<(numPhyReg); j++)
    {
		CPRChkpts[0].unmapped_bits[j] = PRF[j].unmapped_bits;
	}
	CPRChkpts[0].uncomp_instr_counter = 0;
	CPRChkpts[0].load_counter = 0;
	CPRChkpts[0].store_counter = 0;
	CPRChkpts[0].branch_counter = 0;
	CPRChkpts[0].amo = 0;
	CPRChkpts[0].csr = 0;
	CPRChkpts[0].exception = 0;
    
	for (i=1; i<numMaxBranch; i++)
    {
        for (uint64_t j=0; j<(numLogReg); j++)
        {
            CPRChkpts[i].SMT[j].PhyRegMap = 0;
        }
		for (uint64_t j=0; j<(numPhyReg); j++)
		{
			CPRChkpts[i].unmapped_bits[j] = 1;
		}
		CPRChkpts[i].uncomp_instr_counter = 0;
		CPRChkpts[i].load_counter = 0;
		CPRChkpts[i].store_counter = 0;
		CPRChkpts[i].branch_counter = 0;
		CPRChkpts[i].amo = false;
		CPRChkpts[i].csr = false;
		CPRChkpts[i].exception = false;
    }
	CPRHead.value = 0;
	CPRHead.phasebit = 0;
	CPRTail.value = 1;
	CPRTail.phasebit = 0;
}
// Increment the usage counter of physical register “phys_reg”.
void renamer::inc_usage_counter(uint64_t phys_reg)
{
	PRF[phys_reg].usage_counter++;
}

// Decrement the usage counter of physical register “phys_reg”.
// Before decrementing, assert the usage counter > 0.
// After decrementing, check if phys_reg’s unmapped bit is 1 and
// usage counter is 0; if so, push phys_reg onto the Free List.
void renamer::dec_usage_counter(uint64_t phys_reg)
{
	assert(PRF[phys_reg].usage_counter > 0);
	PRF[phys_reg].usage_counter--;
	
	if ((PRF[phys_reg].usage_counter == 0) && (PRF[phys_reg].unmapped_bits == 1))
	{
		FreeList.Entry[FreeList.tail.value].PhyRegNum = phys_reg;
		FreeList.tail.value++;
        if(FreeList.tail.value == (numPhyReg-numLogReg))
        {
            FreeList.tail.value = 0;
            FreeList.tail.phasebit = !FreeList.tail.phasebit;
        }
	}
}

// Clear the unmapped bit of physical register “phys_reg”.
void renamer::map(uint64_t phys_reg)
{
	PRF[phys_reg].unmapped_bits = 0;
}

// Set the unmapped bit of physical register “phys_reg”.
// Check if phys_reg’s usage counter is 0; if so,
// push phys_reg onto the Free List.
void renamer::unmap(uint64_t phys_reg)
{
	PRF[phys_reg].unmapped_bits = 1;
	
	if (PRF[phys_reg].usage_counter == 0)
	{
		FreeList.Entry[FreeList.tail.value].PhyRegNum = phys_reg;
		FreeList.tail.value++;
        if(FreeList.tail.value == (numPhyReg-numLogReg))
        {
            FreeList.tail.value = 0;
            FreeList.tail.phasebit = !FreeList.tail.phasebit;
        }
	}
}


bool renamer::stall_reg(uint64_t bundle_dst)
{
    uint64_t numEmptyFreeList;
    if (FreeList.head.phasebit != FreeList.tail.phasebit) numEmptyFreeList = (numPhyReg - numLogReg) - (FreeList.head.value - FreeList.tail.value);
    else if (FreeList.head.phasebit == FreeList.tail.phasebit) numEmptyFreeList = (FreeList.tail.value - FreeList.head.value);
    
    if (bundle_dst > numEmptyFreeList) return 1;
    else return 0;
}

// bool renamer::stall_branch(uint64_t bundle_branch)
// {
    // uint64_t numEmptyBrChkpts = 0;
    // uint64_t brMask = 1;

    // for (int i=0; i<numMaxBranch; i++)
    // {
        // if ((GBM & brMask)==0) numEmptyBrChkpts++;
        // brMask = brMask << 1;
    // }

    // if (numEmptyBrChkpts < bundle_branch) return 1;
    // else return 0;
// }

// uint64_t renamer::get_branch_mask()
// {
    // return Chkpt_ID;
// }

uint64_t renamer::rename_rsrc(uint64_t log_reg)
{
	inc_usage_counter(RMT[log_reg].PhyRegMap);
    return RMT[log_reg].PhyRegMap;
}

uint64_t renamer::rename_rdst(uint64_t log_reg)
{
	unmap(RMT[log_reg].PhyRegMap);
	
    RMT[log_reg].PhyRegMap = FreeList.Entry[FreeList.head.value].PhyRegNum;
	
	map(RMT[log_reg].PhyRegMap);
	
    FreeList.head.value++;
    if (FreeList.head.value == (numPhyReg-numLogReg)) 
    {
        FreeList.head.value = 0;
        FreeList.head.phasebit = !FreeList.head.phasebit;
    }

	inc_usage_counter(RMT[log_reg].PhyRegMap);
	
    return RMT[log_reg].PhyRegMap;
}

void renamer::checkpoint()
{
	
	MTcopy(RMT,CPRChkpts[CPRTail.value].SMT,numLogReg);
	
	CPRChkpts[CPRTail.value].store_counter = 0;
	CPRChkpts[CPRTail.value].load_counter = 0;
	CPRChkpts[CPRTail.value].branch_counter = 0;
	CPRChkpts[CPRTail.value].uncomp_instr_counter = 0;
	CPRChkpts[CPRTail.value].amo = false;
	CPRChkpts[CPRTail.value].csr = false;
	CPRChkpts[CPRTail.value].exception = false;
	
	for (int i=0; i < numPhyReg; i++)
	{
		CPRChkpts[CPRTail.value].unmapped_bits[i] = PRF[i].unmapped_bits;
	}
	
	for (int i=0; i < numLogReg; i++)
	{
		inc_usage_counter(RMT[i].PhyRegMap);
	}
	
    CPRTail.value++;
    if (CPRTail.value == numMaxBranch) 
    {
        CPRTail.value = 0;
        CPRTail.phasebit = !CPRTail.phasebit;
    }
	
	// Chkpt_ID++;
    // if (Chkpt_ID == numMaxBranch) 
    // {
        // Chkpt_ID = 0;
    // }
}

uint64_t renamer::get_checkpoint_ID(bool load, bool store, bool branch, bool amo, bool csr)
{
	uint64_t Chkpt_ID;
	if (CPRTail.value == 0) Chkpt_ID = numMaxBranch - 1;
	else Chkpt_ID = CPRTail.value - 1;
	
	CPRChkpts[Chkpt_ID].uncomp_instr_counter++;
	if (load) CPRChkpts[Chkpt_ID].load_counter++;
	if (store) CPRChkpts[Chkpt_ID].store_counter++;
	if (branch) CPRChkpts[Chkpt_ID].branch_counter++;
	if (amo) CPRChkpts[Chkpt_ID].amo = true;
	if (csr) CPRChkpts[Chkpt_ID].csr = true;
	return Chkpt_ID;
}

bool renamer::stall_checkpoint(uint64_t bundle_chkpts)
{
    uint64_t numEmptyChkptBuf;
    if (CPRHead.phasebit == CPRTail.phasebit) numEmptyChkptBuf = (numMaxBranch) - (CPRTail.value - CPRHead.value);
    else if (CPRHead.phasebit != CPRTail.phasebit) numEmptyChkptBuf = (CPRHead.value - CPRTail.value);
    
    if (bundle_chkpts > numEmptyChkptBuf) return 1;
    else return 0;
}

void renamer::free_checkpoint()
{
    CPRHead.value++;
    if (CPRHead.value == numMaxBranch) 
    {
        CPRHead.value = 0;
        CPRHead.phasebit = !CPRHead.phasebit;
    }
}


bool renamer::is_ready(uint64_t phys_reg)
{
    return PRF_RdybitArr[phys_reg].readybit;
}

void renamer::clear_ready(uint64_t phys_reg)
{
    PRF_RdybitArr[phys_reg].readybit = 0;
}

uint64_t renamer::read(uint64_t phys_reg)
{
	dec_usage_counter(phys_reg);
    return PRF[phys_reg].value;
}

void renamer::set_ready(uint64_t phys_reg)
{
    PRF_RdybitArr[phys_reg].readybit = 1;
}

void renamer::write(uint64_t phys_reg, uint64_t value)
{
	dec_usage_counter(phys_reg);
    PRF[phys_reg].value = value;
}

void renamer::set_complete(uint64_t CheckpointID)
{
	CPRChkpts[CheckpointID].uncomp_instr_counter--;
}


uint64_t renamer::rollback(uint64_t chkpt_id, bool next, uint64_t &total_loads, uint64_t &total_stores, uint64_t &total_branches){
	uint64_t rollback_chkptid;
	uint64_t squash_chkptid_start;
	uint64_t squash_mask;
	int num_rollbacks;
	uint64_t temp_tail = CPRTail.value;

	if(next) rollback_chkptid = ((chkpt_id + 1)%numMaxBranch);
	else rollback_chkptid = chkpt_id;

	for(int i=0;i<numPhyReg;i++){
		PRF[i].unmapped_bits = CPRChkpts[rollback_chkptid].unmapped_bits[i];
	}

	for(int i=0;i<numLogReg;i++){
		if(RMT[i].PhyRegMap != CPRChkpts[rollback_chkptid].SMT[i].PhyRegMap){
			unmap(RMT[i].PhyRegMap);
		}
		RMT[i].PhyRegMap = CPRChkpts[rollback_chkptid].SMT[i].PhyRegMap;
	}
	
	squash_mask = 1<<rollback_chkptid;
	squash_chkptid_start = ((rollback_chkptid+1)%numMaxBranch);
	
	if(squash_chkptid_start<=CPRTail.value) num_rollbacks = CPRTail.value - squash_chkptid_start;
	else num_rollbacks = numMaxBranch - squash_chkptid_start + CPRTail.value;
	
	for(int i = 0; i<num_rollbacks;i++){
		squash_mask |= (1<<(squash_chkptid_start));
		for(int j=0;j<numLogReg;j++) dec_usage_counter(CPRChkpts[squash_chkptid_start].SMT[j].PhyRegMap);
		squash_chkptid_start = (squash_chkptid_start +1)%numMaxBranch;
	}

	CPRTail.value = (rollback_chkptid+1)%numMaxBranch;
	if(temp_tail<CPRTail.value) CPRTail.phasebit = !CPRTail.phasebit;

	CPRChkpts[rollback_chkptid].branch_counter = 0;
	CPRChkpts[rollback_chkptid].exception = 0;
	CPRChkpts[rollback_chkptid].uncomp_instr_counter = 0;
	CPRChkpts[rollback_chkptid].amo = 0;
	CPRChkpts[rollback_chkptid].store_counter = 0;
	CPRChkpts[rollback_chkptid].csr = 0;
	CPRChkpts[rollback_chkptid].load_counter = 0;
	
	total_loads = 0;
	total_stores = 0;
	total_branches = 0;
	squash_chkptid_start = CPRHead.value;
	if(squash_chkptid_start <= rollback_chkptid-1) num_rollbacks = rollback_chkptid - squash_chkptid_start;
	else num_rollbacks = numMaxBranch - squash_chkptid_start + rollback_chkptid;
	for(int i = 0; i<num_rollbacks;i++){
		total_loads += CPRChkpts[squash_chkptid_start].load_counter;
		total_stores += CPRChkpts[squash_chkptid_start].store_counter;
		total_branches += CPRChkpts[squash_chkptid_start].branch_counter;
		squash_chkptid_start = (squash_chkptid_start +1)%numMaxBranch;
	}
	return squash_mask;
}

bool renamer::precommit(uint64_t &chkpt_id, uint64_t &num_loads, uint64_t &num_stores, uint64_t &num_branches, bool &amo, bool &csr, bool &exception)
{
	if (((((CPRTail.value > (CPRHead.value+1)) && (CPRTail.phasebit == CPRHead.phasebit)) || (((CPRHead.value - CPRTail.value) <= (numMaxBranch-2)) && (CPRTail.phasebit != CPRHead.phasebit))) || (CPRChkpts[CPRHead.value].exception)) && (CPRChkpts[CPRHead.value].uncomp_instr_counter == 0)) {
		chkpt_id = CPRHead.value;
		num_loads = CPRChkpts[CPRHead.value].load_counter;
		num_stores = CPRChkpts[CPRHead.value].store_counter;
		num_branches = CPRChkpts[CPRHead.value].branch_counter;
		amo = CPRChkpts[CPRHead.value].amo;
		csr = CPRChkpts[CPRHead.value].csr;
		exception = CPRChkpts[CPRHead.value].exception;
		return 1;
	}
	else {
		return 0;
	}
}

void renamer::commit(uint64_t log_reg)
{
	dec_usage_counter(CPRChkpts[CPRHead.value].SMT[log_reg].PhyRegMap);
}


void renamer::squash(){
	uint64_t local_rollbackID = (CPRHead.value +1) %numMaxBranch;
	int rollbackID_count;
	uint64_t local_tail = 0;

	for(int i=0;i<numPhyReg;i++) PRF[i].unmapped_bits = CPRChkpts[CPRHead.value].unmapped_bits[i];
	for(int i=0;i<numLogReg;i++){
		if(RMT[i].PhyRegMap != CPRChkpts[CPRHead.value].SMT[i].PhyRegMap) unmap(RMT[i].PhyRegMap);
		RMT[i].PhyRegMap = CPRChkpts[CPRHead.value].SMT[i].PhyRegMap;
	}
	
	if(local_rollbackID<=CPRTail.value)
	{
		rollbackID_count = CPRTail.value - local_rollbackID;
	}
	else 
	{
		rollbackID_count = numMaxBranch - local_rollbackID + CPRTail.value;
	}

	for(int i = 0; i<rollbackID_count;i++){
		for(int j=0;j<numLogReg;j++) dec_usage_counter(CPRChkpts[local_rollbackID].SMT[j].PhyRegMap);
		local_rollbackID = (local_rollbackID +1)%numMaxBranch;
	}

	local_tail = CPRTail.value;
	CPRTail.value = (CPRHead.value+1)%numMaxBranch;
	if(CPRTail.value>local_tail) CPRTail.phasebit = !CPRTail.phasebit;
	
	CPRChkpts[CPRHead.value].branch_counter = 0;
	CPRChkpts[CPRHead.value].exception = 0;
	CPRChkpts[CPRHead.value].uncomp_instr_counter = 0;
	CPRChkpts[CPRHead.value].amo = 0;
	CPRChkpts[CPRHead.value].store_counter = 0;
	CPRChkpts[CPRHead.value].csr = 0;
	CPRChkpts[CPRHead.value].load_counter = 0;
}


void renamer::set_exception(uint64_t CheckpointID)
{
	CPRChkpts[CheckpointID].exception = true;
}

bool renamer::get_exception(uint64_t CheckpointID)
{
    return CPRChkpts[CheckpointID].exception;
}
