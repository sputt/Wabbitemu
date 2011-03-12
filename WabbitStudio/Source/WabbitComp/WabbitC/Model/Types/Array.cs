﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using System.Diagnostics;

namespace WabbitC.Model.Types
{
    class Array : Type
    {
        public Type BaseType;
        public List<int> Dimensions;
        /// <summary>
        /// Array constructor, token list points to first bracket e.g. char buffer*->*[32];
        /// </summary>
        /// <param name="elementType"></param>
        /// <param name="tokenList"></param>
        public Array(Type elementType, ref List<Token>.Enumerator tokens)
        {
            Debug.Assert(tokens.Current == "[");

            BaseType = elementType;
            IndirectionLevels = 0;
            Dimensions = new List<int>();

            int nDimensions = 0;
            while (tokens.Current.Type != TokenType.StatementEnd &&
                tokens.Current != "=")
            {
                if (tokens.Current == "[")
                {
                    tokens.MoveNext();

                    int nElems;
                    if (int.TryParse(tokens.Current.ToString(), out nElems) == true)
                    {
                        if (nElems < 0)
                        {
                            MessageSystem.Instance.ThrowNewError("Negative dimensions are not allowed");
                        }
                        Dimensions.Add(nElems);
                    }
                    else
                    {
                        MessageSystem.Instance.ThrowNewError("Invalid dimension");
                    }

                    tokens.MoveNext();
                    if (tokens.Current == "]")
                    {
                        tokens.MoveNext();
                    }
                    else
                    {
                        MessageSystem.Instance.ThrowNewError("Invalid dimension format (missing ])");
                    }
                }
                nDimensions++;
            }
        }

        public override string ToString()
        {
            var sb = new StringBuilder();
            sb.Append(BaseType);
            foreach (int dim in Dimensions)
            {
                sb.Append("[" + dim + "]");
            }
            return sb.ToString();
        }
    }
}
