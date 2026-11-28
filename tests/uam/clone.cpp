#include "codegen/nv50_ir.h"
#include <cstdio>
using namespace nv50_ir;

int main()
{
    Program program(Program::TYPE_VERTEX, nullptr);
    Function *function = program.main;
    Instruction *load = new_Instruction(function, OP_LOAD, TYPE_U32);
    Symbol *memory = new_Symbol(&program, FILE_MEMORY_GLOBAL, 0);
    memory->reg.data.offset = 156;
    load->setSrc(0, memory);
    load->setDef(0, new_LValue(function, FILE_GPR));
    LValue *address = new_LValue(function, FILE_GPR);
    address->reg.size = 8;
    LValue *index = new_LValue(function, FILE_GPR);
    load->setIndirect(0, 0, address);
    load->setIndirect(0, 1, index);
    Instruction *copy = cloneShallow(function, load);
    bool ok = copy->getSrc(0) == memory && copy->getDef(0) == load->getDef(0);
    for (int dimension = 0; dimension < 2; ++dimension) {
        ok &= copy->src(0).indirect[dimension] == load->src(0).indirect[dimension];
        ok &= copy->getIndirect(0, dimension) == load->getIndirect(0, dimension);
    }
    for (int source = 0; load->srcExists(source); ++source)
        ok &= copy->src(source).usedAsPtr == load->src(source).usedAsPtr;
    std::puts(ok ? "PASS indirect load clone preserves address metadata"
                 : "FAIL indirect load clone loses address metadata");
    return ok ? 0 : 1;
}
