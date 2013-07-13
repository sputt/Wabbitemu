﻿namespace Revsoft.Wabbitcode.Services.Assembler
{

    public enum AssemblyFlags
    {
        None = 0,
        Normal = 1,
        CodeCounter = 2,
        Symtable = 4,
        Stats = 8,
        List = 16,
        Commandline = 32,
    }
}