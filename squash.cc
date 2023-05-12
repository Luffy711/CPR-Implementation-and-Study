#include "pipeline.h"
#include<iostream>
using namespace std;


void pipeline_t::squash_complete(reg_t jump_PC) {
	unsigned int i, j;

	//////////////////////////
	// Fetch Stage
	//////////////////////////
  
	FetchUnit->flush(jump_PC);

	//////////////////////////
	// Decode Stage
	//////////////////////////
	
	for (i = 0; i < fetch_width; i++) {
		DECODE[i].valid = false;
	}
	
	//////////////////////////
	// Rename1 Stage
	//////////////////////////

	FQ.flush();

	//////////////////////////
	// Rename2 Stage
	//////////////////////////

	for (i = 0; i < dispatch_width; i++) {
		RENAME2[i].valid = false;
	}

	//
	// FIX_ME #17c
	// Squash the renamer.
	//

    // FIX_ME #17c BEGIN
	REN->squash();
	instr_renamed_since_last_checkpoint = 0;
	// cout<<"Squash_complete reset: "<<instr_renamed_since_last_checkpoint<<endl;
    // FIX_ME #17c END


	//////////////////////////
	// Dispatch Stage
	//////////////////////////

	for (i = 0; i < dispatch_width; i++) {
		if (DISPATCH[i].valid) {
			if (PAY.buf[DISPATCH[i].index].A_valid) REN->dec_usage_counter(PAY.buf[DISPATCH[i].index].A_phys_reg);
			if (PAY.buf[DISPATCH[i].index].B_valid) REN->dec_usage_counter(PAY.buf[DISPATCH[i].index].B_phys_reg);
			if (PAY.buf[DISPATCH[i].index].C_valid) REN->dec_usage_counter(PAY.buf[DISPATCH[i].index].C_phys_reg);
			if (PAY.buf[DISPATCH[i].index].D_valid) REN->dec_usage_counter(PAY.buf[DISPATCH[i].index].D_phys_reg);
		}
		DISPATCH[i].valid = false;
	}

	//////////////////////////
	// Schedule Stage
	//////////////////////////

	IQ.flush();

	//////////////////////////
	// Register Read Stage
	// Execute Stage
	// Writeback Stage
	//////////////////////////

	for (i = 0; i < issue_width; i++) {
		if (Execution_Lanes[i].rr.valid) {
			if (PAY.buf[Execution_Lanes[i].rr.index].A_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].A_phys_reg);
			if (PAY.buf[Execution_Lanes[i].rr.index].B_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].B_phys_reg);
			if (PAY.buf[Execution_Lanes[i].rr.index].C_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].C_phys_reg);
			if (PAY.buf[Execution_Lanes[i].rr.index].D_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].D_phys_reg);
		}
		Execution_Lanes[i].rr.valid = false;

		for (j = 0; j < Execution_Lanes[i].ex_depth; j++) {
			if (Execution_Lanes[i].ex[j].valid) {
				// if (PAY.buf[Execution_Lanes[i].ex[j].index].A_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].A_phys_reg);
				// if (PAY.buf[Execution_Lanes[i].ex[j].index].B_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].B_phys_reg);
				if (PAY.buf[Execution_Lanes[i].ex[j].index].C_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].C_phys_reg);
				// if (PAY.buf[Execution_Lanes[i].ex[j].index].D_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].D_phys_reg);
			}
			Execution_Lanes[i].ex[j].valid = false;
		}
		
		if (Execution_Lanes[i].wb.valid) {
			// if (PAY.buf[Execution_Lanes[i].wb.index].A_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].A_phys_reg);
			// if (PAY.buf[Execution_Lanes[i].wb.index].B_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].B_phys_reg);
			// if (PAY.buf[Execution_Lanes[i].wb.index].C_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].C_phys_reg);
			// if (PAY.buf[Execution_Lanes[i].wb.index].D_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].D_phys_reg);
		}
		Execution_Lanes[i].wb.valid = false;
	}

	LSU.flush();
}


// void pipeline_t::resolve(uint64_t squash_mask, bool correct) 
void pipeline_t::selective_squash(uint64_t squash_mask) {
	unsigned int i, j;

	// if (correct) {
		// // Instructions in the Rename2 through Writeback Stages have branch masks.
		// // The correctly-resolved branch's bit must be cleared in all branch masks.

		// for (i = 0; i < dispatch_width; i++) {
			// // Rename2 Stage:
			// CLEAR_BIT(squash_mask, RENAME2[i].checkpoint_id);

			// // Dispatch Stage:
			// CLEAR_BIT(squash_mask, DISPATCH[i].checkpoint_id);
		// }

		// // Schedule Stage:
		// IQ.clear_branch_bit(squash_mask);

		// for (i = 0; i < issue_width; i++) {
			// // Register Read Stage:
			// CLEAR_BIT(squash_mask, Execution_Lanes[i].rr.checkpoint_id);

			// // Execute Stage:
			// for (j = 0; j < Execution_Lanes[i].ex_depth; j++)
			   // CLEAR_BIT(squash_mask, Execution_Lanes[i].ex[j].checkpoint_id);

			// // Writeback Stage:
			// CLEAR_BIT(squash_mask, Execution_Lanes[i].wb.checkpoint_id);
		// }
	// }
	// else {
		// Squash all instructions in the Decode through Dispatch Stages.

		// Decode Stage:
		for (i = 0; i < fetch_width; i++) {
			// if (DECODE[i].valid) {
				// if (PAY.buf[DECODE[i].index].A_valid) REN->dec_usage_counter(PAY.buf[DECODE[i].index].A_phys_reg);
				// if (PAY.buf[DECODE[i].index].B_valid) REN->dec_usage_counter(PAY.buf[DECODE[i].index].B_phys_reg);
				// if (PAY.buf[DECODE[i].index].C_valid) REN->dec_usage_counter(PAY.buf[DECODE[i].index].C_phys_reg);
				// if (PAY.buf[DECODE[i].index].D_valid) REN->dec_usage_counter(PAY.buf[DECODE[i].index].D_phys_reg);
			// }
			DECODE[i].valid = false;
		}

		// Rename1 Stage:
		FQ.flush();

		// Rename2 Stage:
		for (i = 0; i < dispatch_width; i++) {
			// if (RENAME2[i].valid) {
				// if (PAY.buf[RENAME2[i].index].A_valid) REN->dec_usage_counter(PAY.buf[RENAME2[i].index].A_phys_reg);
				// if (PAY.buf[RENAME2[i].index].B_valid) REN->dec_usage_counter(PAY.buf[RENAME2[i].index].B_phys_reg);
				// if (PAY.buf[RENAME2[i].index].C_valid) REN->dec_usage_counter(PAY.buf[RENAME2[i].index].C_phys_reg);
				// if (PAY.buf[RENAME2[i].index].D_valid) REN->dec_usage_counter(PAY.buf[RENAME2[i].index].D_phys_reg);
			// }
			RENAME2[i].valid = false;
		}
		

		// Dispatch Stage:
		for (i = 0; i < dispatch_width; i++) {
			if (DISPATCH[i].valid) {
				if (PAY.buf[DISPATCH[i].index].A_valid) REN->dec_usage_counter(PAY.buf[DISPATCH[i].index].A_phys_reg);
				if (PAY.buf[DISPATCH[i].index].B_valid) REN->dec_usage_counter(PAY.buf[DISPATCH[i].index].B_phys_reg);
				if (PAY.buf[DISPATCH[i].index].C_valid) REN->dec_usage_counter(PAY.buf[DISPATCH[i].index].C_phys_reg);
				if (PAY.buf[DISPATCH[i].index].D_valid) REN->dec_usage_counter(PAY.buf[DISPATCH[i].index].D_phys_reg);
			}
			DISPATCH[i].valid = false;
		}

		// Selectively squash instructions after the branch, in the Schedule through Writeback Stages.

		// Schedule Stage:
		IQ.squash(squash_mask);

		for (i = 0; i < issue_width; i++) {
			// Register Read Stage:
			if (Execution_Lanes[i].rr.valid && BIT_IS_ONE(squash_mask, Execution_Lanes[i].rr.checkpoint_id)) {
				if (PAY.buf[Execution_Lanes[i].rr.index].A_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].A_phys_reg);
				if (PAY.buf[Execution_Lanes[i].rr.index].B_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].B_phys_reg);
				if (PAY.buf[Execution_Lanes[i].rr.index].C_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].C_phys_reg);
				if (PAY.buf[Execution_Lanes[i].rr.index].D_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].D_phys_reg);
				Execution_Lanes[i].rr.valid = false;
			}

			// Execute Stage:
			for (j = 0; j < Execution_Lanes[i].ex_depth; j++) {
			   if (Execution_Lanes[i].ex[j].valid && BIT_IS_ONE(squash_mask, Execution_Lanes[i].ex[j].checkpoint_id)) {
				// if (PAY.buf[Execution_Lanes[i].ex[j].index].A_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].A_phys_reg);
				// if (PAY.buf[Execution_Lanes[i].ex[j].index].B_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].B_phys_reg);
				if (PAY.buf[Execution_Lanes[i].ex[j].index].C_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].C_phys_reg);
				// if (PAY.buf[Execution_Lanes[i].ex[j].index].D_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].D_phys_reg);
				Execution_Lanes[i].ex[j].valid = false;
			   }
			}

			// Writeback Stage:
			if (Execution_Lanes[i].wb.valid && BIT_IS_ONE(squash_mask, Execution_Lanes[i].wb.checkpoint_id)) {
				// if (PAY.buf[Execution_Lanes[i].wb.index].A_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].A_phys_reg);
				// if (PAY.buf[Execution_Lanes[i].wb.index].B_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].B_phys_reg);
				// if (PAY.buf[Execution_Lanes[i].wb.index].C_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].C_phys_reg);
				// if (PAY.buf[Execution_Lanes[i].wb.index].D_valid) REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].D_phys_reg);
				Execution_Lanes[i].wb.valid = false;
			}
		}
		instr_renamed_since_last_checkpoint = 0;
		// cout<<"Squash_sel reset: "<<instr_renamed_since_last_checkpoint<<endl;
	// }
}
