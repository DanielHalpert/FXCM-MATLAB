Using Forex Connect API with Matlab

The fc_matlab.dll allows to connect to FXCM server and get real time and historical prices, 
place market, limit and stop orders.
All functions used by ForexConnect are asynchronous, so the fc_matlab.dll implements an event-driven 
architecture it receives messages/events from FXCM server and pass them to fc_mexCallback function,
from there it get passed to Matlab functions.
To start the FC API first open the FC_login.m, select Run, select Change Folder so the fc_matlab_run 
is the current Matlab folder.
To login you need FXCM Demo or Real account, to get a demo account it can be done from fxcm.com.

the currect DLL works only with Matlab 64 bit 
the project build with Visual Studio 2015

if you don't have VS 2015 installed please download the Visual C++ Redistributable for Visual Studio 2015
https://www.microsoft.com/en-us/download/details.aspx?id=48145

For any FXCM API question please email to api@fxcm.com

