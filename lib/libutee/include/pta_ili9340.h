/*
 * Copyright (C) Microsoft. All rights reserved
 */

#ifndef __PTA_ILI9340_H
#define __PTA_ILI9340_H

/*
 * Interface to the ILI9340 pseudo-TA.
 */

                           
// {5FB6F3D5-5592-41DB-B24D-2605B1603E56}
#define PTA_ILI9340_UUID { 0x5fb6f3d5, 0x5592, 0x41db, { \
                           0xb2, 0x4d, 0x26, 0x5, 0xb1, 0x60, 0x3e, 0x56 } }

enum ILI9340_WIRE_BYTE_TYPE {
    ILI9340_TYPE_COMMAND = 0,
    ILI9340_TYPE_DATA = 1,
};

enum ILI9340_PTA_COMMANDS {
 
    /*
     * Description: Device initialization
     * 
     */ 
    PTA_ILI9340_INIT  = 0,

    /*
     * Description: transfer a single command or data
     * 
     * [in] params[0].value.a: Command or data value
     * [in] params[0].value.b: Command (0), or Data (<>0)
     * [out optional] params[1].value.b: Read value
     */ 
    PTA_ILI9340_XCHG_SINGLE = 1,

    /*
     * Description: transfer command and data
     * 
     * [in] params[0].value.a: Command or data value
     * [in] params[0].value.b: Is command present (1), otherwize 0
     * [in  optional] params[1].memref: Data values to send
     * [out optional] params[2].memref: Received data values
     */ 
    PTA_ILI9340_XCHG = 2
};
#endif /* __PTA_ILI9340_H */
