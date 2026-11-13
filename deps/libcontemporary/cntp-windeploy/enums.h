#ifndef ENUMS_H
#define ENUMS_H

#include <QtGlobal>

enum class TargetMachine : quint16 {
    Unknown = 0x0,
    Amd64 = 0x8664,
    Arm = 0x01c0,
    Arm64 = 0xaa64,
    Efi = 0x0ebc,
    I386 = 0x014c,
    IA64 = 0x0200,
    Mips16 = 0x0266,
    MipsWithFpu = 0x0366,
    Mips16WithFpu = 0x466,
    PowerPc = 0x01f0
};

#endif // ENUMS_H
