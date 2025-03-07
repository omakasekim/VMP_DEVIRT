#include "pin.H"
#include <fstream>
#include <vector>
#include <map>
#include <mutex>
#include <string>

// Global output file – logs to "trace.log"
std::ofstream traceFile("trace.log");

// Vector to store VMP sections as (start, end) pairs.
std::vector< std::pair<ADDRINT, ADDRINT> > vmpSections;

// Map to store per-thread instruction counters.
std::map<THREADID, UINT32> instructionCounters;

// Mutex to protect shared data.
std::mutex traceMutex;

// Global flag to indicate whether tracing is enabled.
bool tracingEnabled = false;

// Threshold after which tracing is enabled.
UINT32 threshold = 10000;

/**
 * writeTrace - Logs the current instruction, its disassembly, and registers.
 * Note: For memory read/write, it uses a questionable method (reading REG_INST_PTR)
 * which you may wish to replace with a safer mechanism.
 */
VOID writeTrace(THREADID tid, const CONTEXT* ctx, INS ins) {
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
    
    // Log memory read (if applicable)
    if (INS_IsMemoryRead(ins)) {
        ADDRINT memAddr;
        // WARNING: This retrieves REG_INST_PTR (not the memory read EA). Replace with a safer routine if needed.
        PIN_GetContextRegval(ctx, REG_INST_PTR, reinterpret_cast<UINT8*>(&memAddr));
        traceFile << "MEMORY_READ: " << memAddr << "=" << *(UINT64*)memAddr << " ";
    }
    // Log memory write (if applicable)
    if (INS_IsMemoryWrite(ins)) {
        ADDRINT memAddr;
        PIN_GetContextRegval(ctx, REG_INST_PTR, reinterpret_cast<UINT8*>(&memAddr));
        traceFile << "MEMORY_WRITE: " << memAddr << "=" << *(UINT64*)memAddr << " ";
    }
    traceFile << "\n";
}

/**
 * findVmpSections - Iterates over sections in an image and stores sections whose name starts with ".vmp".
 */
VOID findVmpSections(IMG img, VOID* v) {
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        std::string secName = SEC_Name(sec);
        if (secName.find(".vmp") == 0) {
            ADDRINT start = SEC_Address(sec);
            ADDRINT end = start + SEC_Size(sec);
            vmpSections.push_back({start, end});
        }
    }
}

/**
 * isInVmpSection - Returns true if the given address is within one of the VMP sections.
 */
bool isInVmpSection(ADDRINT addr) {
    for (const auto& section : vmpSections) {
        if (addr >= section.first && addr <= section.second)
            return true;
    }
    return false;
}

/**
 * checkMemoryRead - Enables tracing if a memory read occurs in a VMP section.
 */
VOID checkMemoryRead(ADDRINT addr) {
    if (isInVmpSection(addr)) {
        std::lock_guard<std::mutex> lock(traceMutex);
        tracingEnabled = true;
    }
}

/**
 * incrementCounter - Increments the instruction counter for the given thread and enables tracing
 * if the threshold is exceeded.
 */
VOID incrementCounter(THREADID tid) {
    std::lock_guard<std::mutex> lock(traceMutex);
    instructionCounters[tid]++; 
    if (instructionCounters[tid] > threshold) {
        tracingEnabled = true;
    }
}

/**
 * onFunctionEntry - Instruments calls to increment the instruction counter.
 */
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

/**
 * onInstruction - For each instruction, if it reads memory, checks if the read is in a VMP section.
 * Also logs the instruction.
 */
VOID onInstruction(INS ins, VOID* v) {
    if (INS_IsMemoryRead(ins)) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)checkMemoryRead,
                                 IARG_MEMORYREAD_EA, IARG_END);
    }
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeTrace,
                   IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_PTR, ins, IARG_END);
}

/**
 * onFunctionExit - On return instructions, if tracing is enabled, disable it.
 */
VOID onFunctionExit(TRACE trace, VOID* v) {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            if (INS_IsRet(ins) && tracingEnabled) {
                tracingEnabled = false;
            }
        }
    }
}

/**
 * ImageLoad - Called for each image load. It calls findVmpSections to record VMP sections.
 */
VOID ImageLoad(IMG img, VOID* v) {
    findVmpSections(img, v);
}

int main(int argc, char* argv[]) {
    // Initialize symbol processing.
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) return 1;

    IMG_AddInstrumentFunction(ImageLoad, 0);
    TRACE_AddInstrumentFunction(onFunctionEntry, 0);
    TRACE_AddInstrumentFunction(onFunctionExit, 0);
    INS_AddInstrumentFunction(onInstruction, 0);

    PIN_StartProgram();
    return 0;
}
