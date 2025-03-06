#include "pin.H"
#include <fstream>
#include <vector>
#include <map>
#include <mutex>
#include <string>

std::ofstream traceFile("trace.log");
std::vector<std::pair<ADDRINT, ADDRINT>> vmpSections;
std::map<THREADID, UINT32> instructionCounters;
std::mutex traceMutex;
bool tracingEnabled = false;
UINT32 threshold = 10000;

void writeTrace(THREADID tid, const CONTEXT* ctx, INS ins) {
    std::lock_guard<std::mutex> lock(traceMutex);
    if (!tracingEnabled) return;

    traceFile << "RIP: " << std::hex << INS_Address(ins)
              << " OPCODE: " << INS_Disassemble(ins) << " REGISTERS: ";

    traceFile << "RAX=" << PIN_GetContextReg(ctx, REG_RAX) << " "
              << "RBX=" << PIN_GetContextReg(ctx, REG_RBX) << " "
              << "RCX=" << PIN_GetContextReg(ctx, REG_RCX) << " "
              << "RDX=" << PIN_GetContextReg(ctx, REG_RDX) << " "
              << "RSI=" << PIN_GetContextReg(ctx, REG_RSI) << " "
              << "RDI=" << PIN_GetContextReg(ctx, REG_RDI) << " "
              << "RBP=" << PIN_GetContextReg(ctx, REG_RBP) << " "
              << "RSP=" << PIN_GetContextReg(ctx, REG_RSP) << " ";

    if (INS_IsMemoryRead(ins)) {
        ADDRINT memAddr;
        PIN_GetContextRegval(ctx, REG_INST_PTR, reinterpret_cast<UINT8*>(&memAddr));
        traceFile << "MEMORY_READ: " << memAddr << "=" << *(UINT64*)memAddr << " ";
    }
    if (INS_IsMemoryWrite(ins)) {
        ADDRINT memAddr;
        PIN_GetContextRegval(ctx, REG_INST_PTR, reinterpret_cast<UINT8*>(&memAddr));
        traceFile << "MEMORY_WRITE: " << memAddr << "=" << *(UINT64*)memAddr << " ";
    }
    traceFile << "\n";
}

void findVmpSections(IMG img, VOID* v) {
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        std::string secName = SEC_Name(sec);
        if (secName.find(".vmp") == 0) {
            ADDRINT start = SEC_Address(sec);
            ADDRINT end = start + SEC_Size(sec);
            vmpSections.push_back({start, end});
        }
    }
}

bool isInVmpSection(ADDRINT addr) {
    for (const auto& section : vmpSections) {
        if (addr >= section.first && addr <= section.second) return true;
    }
    return false;
}

VOID checkMemoryRead(ADDRINT addr) {
    if (isInVmpSection(addr)) {
        std::lock_guard<std::mutex> lock(traceMutex);
        tracingEnabled = true;
    }
}

VOID incrementCounter(THREADID tid) {
    if (instructionCounters.find(tid) == instructionCounters.end()) {
        instructionCounters[tid] = 0;
    }
    instructionCounters[tid]++;
    if (instructionCounters[tid] > threshold) {
        tracingEnabled = true;
    }
}

VOID onFunctionEntry(TRACE trace, VOID* v) {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            if (INS_IsCall(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)incrementCounter,
                               IARG_THREAD_ID, IARG_END);
            }
        }
    }
}

VOID onInstruction(INS ins, VOID* v) {
    if (INS_IsMemoryRead(ins)) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)checkMemoryRead,
                                 IARG_MEMORYREAD_EA, IARG_END);
    }
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeTrace,
                   IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_PTR, ins, IARG_END);
}

VOID onFunctionExit(TRACE trace, VOID* v) {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            if (INS_IsRet(ins) && tracingEnabled) {
                tracingEnabled = false;
            }
        }
    }
}

VOID ImageLoad(IMG img, VOID* v) {
    findVmpSections(img, v);
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) return 1;

    IMG_AddInstrumentFunction(findVmpSections, 0);
    TRACE_AddInstrumentFunction(onFunctionEntry, 0);
    TRACE_AddInstrumentFunction(onFunctionExit, 0);
    INS_AddInstrumentFunction(onInstruction, 0);

    PIN_StartProgram();
    return 0;
}