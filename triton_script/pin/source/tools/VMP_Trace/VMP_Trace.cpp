#include <iostream>
#include <fstream>
#include "pin.H"


// If PIN_InitSymbolsAlt is not defined, fall back to PIN_InitSymbols.
#ifndef PIN_InitSymbolsAlt
#define PIN_InitSymbolsAlt(mode) PIN_InitSymbols()
#endif

using std::cerr;
using std::endl;
using std::hex;
using std::dec;

// In 64-bit builds, UINT64 is defined in pin.H (typically as ADDRINT).
static std::ofstream TraceFile;
static bool g_tracingActive = false;

// Use UINT64 for address values.
static UINT64 mainImageLow = 0;
static UINT64 mainImageHigh = 0;

// Called on call instructions to start tracing.
VOID AtCall(THREADID tid, UINT64 ip, UINT64 target)
{
    if (!g_tracingActive)
    {
        if (target >= mainImageLow && target < mainImageHigh)
        {
            g_tracingActive = true;
            TraceFile << "[+] Enter function at 0x" << hex << target << dec << endl;
        }
    }
}

// Called on return instructions to end tracing.
VOID AtReturn(THREADID tid, UINT64 ip, UINT64 target)
{
    if (g_tracingActive)
    {
        if (target >= mainImageLow && target < mainImageHigh)
        {
            TraceFile << "[-] Exit function at 0x" << hex << ip << dec << endl;
            g_tracingActive = false;
        }
    }
}

// Log an instruction pointer.
VOID LogInstruction(VOID* ip)
{
    if (g_tracingActive)
    {
        TraceFile << "i:0x" << hex << reinterpret_cast<UINT64>(ip) << dec << endl;
    }
}

// Log memory read events.
VOID LogMemRead(VOID* ip, VOID* addr)
{
    if (g_tracingActive)
    {
        TraceFile << "mr:0x" << hex << reinterpret_cast<UINT64>(ip)
                  << ":0x" << reinterpret_cast<UINT64>(addr) << dec << endl;
    }
}

// Log memory write events.
VOID LogMemWrite(VOID* ip, VOID* addr)
{
    if (g_tracingActive)
    {
        TraceFile << "mw:0x" << hex << reinterpret_cast<UINT64>(ip)
                  << ":0x" << reinterpret_cast<UINT64>(addr) << dec << endl;
    }
}

// Instrument each instruction.
VOID Instruction(INS ins, VOID* v)
{
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)LogInstruction,
                   IARG_INST_PTR, IARG_END);
    if (INS_IsMemoryRead(ins))
    {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)LogMemRead,
                       IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_END);
    }
    if (INS_IsMemoryWrite(ins))
    {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)LogMemWrite,
                       IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_END);
    }
}

// Instrument basic blocks to hook calls and returns.
VOID Trace(TRACE trace, VOID* v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            if (INS_IsCall(ins))
            {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtCall,
                               IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
            }
            else if (INS_IsRet(ins))
            {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtReturn,
                               IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_END);
            }
        }
    }
}

// Record the main executable's address range.
VOID ImageLoad(IMG img, VOID* v)
{
    if (IMG_IsMainExecutable(img))
    {
        mainImageLow  = IMG_LowAddress(img);
        mainImageHigh = IMG_HighAddress(img);
    }
}

// Close the trace file at program termination.
VOID Fini(INT32 code, VOID* v)
{
    if (TraceFile.is_open())
    {
        TraceFile.close();
    }
}

int main(int argc, char* argv[])
{
    if (PIN_Init(argc, argv))
    {
        cerr << "PIN_Init failed." << endl;
        return 1;
    }
    
    // For 64-bit Pin 3.21.1, call PIN_InitSymbolsAlt.
    // Use static_cast to disambiguate UINT32.
    PIN_InitSymbolsAlt(SYMBOL_INFO_MODE(static_cast<UINT32>(IFUNC_SYMBOLS) |
                                        static_cast<UINT32>(DEBUG_OR_EXPORT_SYMBOLS)));
    
    // Open the output file (must be named "filename.vmp.trace" for attack_vmp.py).
    TraceFile.open("filename.vmp.trace");
    if (!TraceFile.is_open())
    {
        cerr << "Failed to open trace file!" << endl;
        return 1;
    }
    TraceFile << "Starting automatic trace..." << endl;
    
    IMG_AddInstrumentFunction(ImageLoad, 0);
    TRACE_AddInstrumentFunction(Trace, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    
    PIN_StartProgram();
    return 0;
}
