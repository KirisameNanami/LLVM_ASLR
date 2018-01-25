#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/LiveVariables.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;
//using namespace std;

namespace {
    struct X86MyASLR : public MachineFunctionPass {
        public:
            static char ID;
            X86MyASLR() : MachineFunctionPass(ID) {}
            bool runOnMachineFunction(MachineFunction &MF) override;
        private:
            bool applyASLR(MachineFunction &MF, MachineFunction::iterator MFI);
            const TargetInstrInfo *TII;
            const TargetRegisterInfo *TRI;
    };
    char X86MyASLR::ID = 0;
}

bool X86MyASLR::runOnMachineFunction(MachineFunction &Func) {
    if(Func.hasInlineAsm()) return false;
    TII = Func.getSubtarget().getInstrInfo();
    TRI = Func.getSubtarget().getRegisterInfo();
    printf("X86MyASLR %s\n", Func.getName().data());
    bool modified = false;
    for(MachineFunction::iterator I = Func.begin(); I != Func.end(); I++) {
        modified |= applyASLR(Func, I);
    }
    return modified;
}

bool X86MyASLR::applyASLR(MachineFunction &MF, MachineFunction::iterator MFI) {
    bool hasUncondBrOrRet = false;
    bool needUncondBr = false;
    bool modified = false;

    for(MachineBasicBlock::iterator I = MFI->getFirstTerminator(); I != MFI->end(); I++) {
        if(I->isUnconditionalBranch() || I->isReturn() || I->isIndirectBranch()) {
            hasUncondBrOrRet = true;
        } else if (I->isConditionalBranch() || !needUncondBr) {
            MachineBasicBlock::iterator J = I;
            do {
                J++;
            } while(J != MFI->end()
                        && J->isTerminator()
                        && !J->isReturn()
                        && !J->isUnconditionalBranch()
                        && !J->isIndirectBranch());
            // fall through. add "JMP to the next MBB"
            if (J == MFI->end()) {
                needUncondBr = true;
            } else if (!J->isTerminator()) assert(J->isTerminator()); // preserved just in case
        }
    }
    
    if (needUncondBr || !hasUncondBrOrRet) {
        MachineFunction::iterator nextMFI = MFI;
        ++nextMFI;

        if (nextMFI == MF.end()) {
            BuildMI(*MFI, MFI->end(),
                        MFI->getLastNonDebugInstr()->getDebugLoc(), TII->get(X86::RETQ));
        } else {
            BuildMI(*MFI, MFI->end(),
                        MFI->getLastNonDebugInstr()->getDebugLoc(), TII->get(X86::JMP_1))
                .addMBB(&*nextMFI);
        }
        modified = true;
    }

    return modified;
}

FunctionPass *llvm::createX86MyASLR() { return new X86MyASLR(); }
