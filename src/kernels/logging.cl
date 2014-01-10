typedef struct tag_log_writer
{
    volatile __global int* pDebugBuffer; 
    int  valuesToWrite; // Stores how many report values are still expected
    int  WriteIdx;      // Stores where should the next value be written
    volatile __global int* dataTypePtr; // Stores the position of the dataType storage
} log_writer;

// debug buffer structure:
//   int32       msgLength  message total length (including header)
//   int32       msgCoding  message formatting code
//   int32/float data0
//   int32/float data1
//   ...
//   int32/float dataN      N = valueCount)

log_writer logStart(volatile __global int* debugBuf, int msgCoding, int valueCount)
{
    // Create writer struct
    log_writer ret;
    ret.pDebugBuffer = debugBuf;
    ret.valuesToWrite = valueCount;

    // Compute message total size
    int msgLength = 1/*msgLength*/ + 1/*msgCoding*/ + valueCount; 
    
    // Allocate space in buffer
    ret.WriteIdx = atomic_add(debugBuf, msgLength);
    
    // Write msgLength and msgCoding
    ret.pDebugBuffer[1 + (ret.WriteIdx++ % (LOG_SIZE - 1))] = msgLength;
    ret.pDebugBuffer[1 + (ret.WriteIdx++ % (LOG_SIZE - 1))] = msgCoding;
    
    return ret;
}

void logValue(log_writer* writer, float value)
{
    // Check if we allocated enough space
    if (writer->valuesToWrite == 0)
        return;
        
    // Update write counter
    writer->valuesToWrite--;
    
    // Write value
    writer->pDebugBuffer[1 + (writer->WriteIdx++ % (LOG_SIZE - 1))] = as_int(value);
}

void logPrint(volatile __global int *debugBuf, int msgCode)
{
    logStart(debugBuf, msgCode, 0);
}

void logPrintf1(volatile __global int *debugBuf, int msgCode, float param1)
{
    log_writer w = logStart(debugBuf, msgCode, 1);
    logValue(&w, param1);
}

void logPrintf2(volatile __global int *debugBuf, int msgCode, float param1, float param2)
{
    log_writer w = logStart(debugBuf, msgCode, 2);
    logValue(&w, param1);
    logValue(&w, param2);
}

void logPrintf3(volatile __global int *debugBuf, int msgCode, float param1, float param2, float param3)
{
    log_writer w = logStart(debugBuf, msgCode, 3);
    logValue(&w, param1);
    logValue(&w, param2);
    logValue(&w, param3);
}

void logPrintf4(volatile __global int *debugBuf, int msgCode, float param1, float param2, float param3, float param4)
{
    log_writer w = logStart(debugBuf, msgCode, 4);
    logValue(&w, param1);
    logValue(&w, param2);
    logValue(&w, param3);
    logValue(&w, param4);
}

void logPrintf5(volatile __global int *debugBuf, int msgCode, float param1, float param2, float param3, float param4, float param5)
{
    log_writer w = logStart(debugBuf, msgCode, 5);
    logValue(&w, param1);
    logValue(&w, param2);
    logValue(&w, param3);
    logValue(&w, param4);
    logValue(&w, param5);
}

#define TextToID(a) (a)

