/* Copyright (c) 2006-2015, Universities Space Research Association (USRA).
*  All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the Universities Space Research Association nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY USRA ``AS IS'' AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL USRA BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package model.expr;

import java.util.Vector;
import main.Log;
import model.GlobalDeclList;
import model.Node;

public class NumericComparison
    extends GeneralExpr {

    public NumericComparison(String op, Expr left, Expr right) {
        super(op);
        addSubExpr(left);
        addSubExpr(right);
    }

    public ExprType getType() {
        return ExprType.Bool;
    }

    /**
     * @brief Check the expression for type and other errors.
     * @param n The node providing the variable binding context.
     * @param decls The plan's global declarations.
     * @param contextMsg String to append to any error messages generated.
     * @param errors (in/out parameter) Collection of errors recorded.
     */
    public ExprType check(Node n,
                          GlobalDeclList decls,
                          String contextMsg,
                          Vector<Log> errors) {
        Expr le = subexprs.get(0);
        Expr re = subexprs.get(1);

        ExprType lt = le.check(n, decls, contextMsg, errors);
        ExprType rt = re.check(n, decls, contextMsg, errors);

        checkNumericType(le, lt, operator, contextMsg, errors);
        checkNumericType(re, rt, operator, contextMsg, errors);

        return ExprType.Bool;
    }

}
