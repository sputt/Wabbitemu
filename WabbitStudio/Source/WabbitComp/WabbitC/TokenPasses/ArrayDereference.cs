﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace WabbitC.TokenPasses
{
    class ArrayDereference : TokenPass
    {
        public List<Token> Run(List<Token> tokenList)
        {
            return tokenList;
        }
    }
}
