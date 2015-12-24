//------------------------------
// to compile this program use at prompt:
// mex -v fc_mexCallback.c fc_matlab.lib
// to run it: fc_mexCallback
//------------------------------


#include "mex.h"
#include "fc_matlab.h"

// Callback function
void fc_mexCallback(char *msg, double msgType)
{
    int k = (int)msgType;
    mxArray *lhs[2];

    lhs[0] =  mxCreateCellMatrix(1,2);
    
    lhs[0] = mxCreateString(msg);
    lhs[1] = mxCreateDoubleScalar(msgType);
      
    //mexPrintf("mexPrintf: %s, %d\n", msg, k); 
    
    //call Matlab function to pring fcDLL messages from server
    switch(k)
    {
        case 1: mexCallMATLAB(0, NULL, 2, &lhs, "fc_pringMsgSystem");
                break;
        case 2: mexCallMATLAB(0, NULL, 2, &lhs, "fc_pringMsgTradingReports");
                break;
        case 3: mexCallMATLAB(0, NULL, 2, &lhs, "fc_pringMsgPrices");
                break;
    }
    
}
// MEX Gateway
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    //pointer to callback function
    void (*cbPtr)() = NULL;
    cbPtr = fc_mexCallback;
    //logout_fc();
    Register_Callback(cbPtr);
}
