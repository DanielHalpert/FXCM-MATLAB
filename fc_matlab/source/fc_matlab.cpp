// fcDLL.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "fc_matlab2.h"
#include "fc_matlab.h"
#include "OrderMonitor.h"
#include "ResponseListener.h"
#include "SessionStatusListener.h"
#include "LoginParams.h"
#include "CommonSources.h"
#include <iostream>
#include <fstream>
#include <string>

void printPrices(IO2GResponse *response, const char* fileName);
IO2GRequest *createMarketOrderRequest(IO2GSession* session, const char* sOfferID, const char* sAccountID, int iAmount, const char* sBuySell, const char* tif, const char* ticketNum);
IO2GRequest *createEntryOrderRequest(IO2GSession *session, const char *sOfferID, const char *sAccountID, int iAmount, const char *sBuySell, const char* StopLimit, const double price);
IO2GRequest *createELSRequest(IO2GSession *session, const char *sOfferID, const char *sAccountID, int iAmount, double dRate, double dRateLimit, double dRateStop, const char *sBuySell, const char *sOrderType, const char * tif, const int trailStop);

char* getAccountID(IO2GSession *session, const char *sAccount);

static IO2GSession *session=NULL;
static ResponseListener *responseListener=NULL;
static SessionStatusListener *sessionListener=NULL;
int recNum=0;
std::ofstream outFile;

static fnCallBackFunc cb_func = NULL;  //callback function pointer
bool bWasError = false;


int __stdcall  login_fc(const char* username, const char* pass, const char* connection, const char* url)
{
	LoginParams *loginParams = new LoginParams(username, pass, connection, url);

    session = CO2GTransport::createSession();

    sessionListener = new SessionStatusListener(session, true, "", "");
    session->subscribeSessionStatus(sessionListener);

    bool bConnected = login(session, sessionListener, loginParams);

    if (bConnected)
    {
        responseListener = new ResponseListener(session);
        session->subscribeResponse(responseListener);
	}

	return(1);
}

int __stdcall logout_fc()
{
	printLog("unsubscribeResponse(responseListener)");
	session->unsubscribeResponse(responseListener);

	printLog("responseListener->release");
	responseListener->release();

	printLog("logout");
	logout(session, sessionListener);
	
	printLog("unsubscribeSessionStatus(sessionListener)");
	session->unsubscribeSessionStatus(sessionListener);

	printLog("sessionListener->release()");
    sessionListener->release();

	printLog("session->release()");
    session->release();
	
	return(1);
}

void printLog(std::string str)
{
	SYSTEMTIME lpSystemTime;
	GetSystemTime(&lpSystemTime);
	std::cout << lpSystemTime.wHour << ":" << lpSystemTime.wMinute << ":" << lpSystemTime.wSecond << "," << lpSystemTime.wMilliseconds << " : " << str << "\n";
}
//------------------------------------------
//------------------------------------------
//get historical prices
bool __stdcall getHistoryPrices(const char *sInstrument, const char *sTimeframe,  const char* dtFrom1,  const char* dtTo1, const char* fileName)
{
    O2G2Ptr<IO2GRequestFactory> factory = session->getRequestFactory();
    if (!factory)
    {
        std::cout << "Cannot create request factory" << std::endl;
        return false;
    }
	recNum = 0;
	std::string sDateFrom = dtFrom1;
	std::string sDateTo = dtTo1;
	struct tm tmBuf = {0};
	DATE dtFrom, dtTo;
	strptime(sDateFrom.c_str(), "%m.%d.%Y %H:%M:%S", &tmBuf);
	CO2GDateUtils::CTimeToOleTime(&tmBuf, &dtFrom);

	strptime(sDateTo.c_str(), "%m.%d.%Y %H:%M:%S", &tmBuf);
	CO2GDateUtils::CTimeToOleTime(&tmBuf, &dtTo);
	
	//std::cout << sDateTo.c_str() << std::endl;

	if (dtFrom >= dtTo)
	{
		std::cout << "Request faild, Datefrom should be < DateTo" << std::endl;
        return false;
	}

    //find timeframe by identifier
    O2G2Ptr<IO2GTimeframeCollection> timeframeCollection = factory->getTimeFrameCollection();
    O2G2Ptr<IO2GTimeframe> timeframe = timeframeCollection->get(sTimeframe);
    if (!timeframe)
    {
        std::cout << "Timeframe '" << sTimeframe << "' is incorrect!" << std::endl;
        return false;
    }
    O2G2Ptr<IO2GRequest> request = factory->createMarketDataSnapshotRequestInstrument(sInstrument, timeframe, timeframe->getQueryDepth());
    DATE dtFirst = dtTo;
    // there is limit for returned candles amount
    do
    {
        factory->fillMarketDataSnapshotRequestTime(request, dtFrom, dtFirst, false);
        responseListener->setRequestID(request->getRequestID());
        session->sendRequest(request);
        if (!responseListener->waitEvents())
        {
            std::cout << "Response waiting timeout expired" << std::endl;
            return false;
        }
        // shift "to" bound to oldest datetime of returned data
        O2G2Ptr<IO2GResponse> response = responseListener->getResponse();
        if (response && response->getType() == MarketDataSnapshot)
        {
            O2G2Ptr<IO2GResponseReaderFactory> readerFactory = session->getResponseReaderFactory();
            if (readerFactory)
            {
                O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = readerFactory->createMarketDataSnapshotReader(response);
                if (reader->size() > 0)
                {
                    if (abs(dtFirst - reader->getDate(0)) > 0.0001)
                        dtFirst = reader->getDate(0); // earliest datetime of returned data
                    else
                        break;
                }
                else
                {
                    std::cout << "0 rows received" << std::endl;
                    break;
                }
            }
            printPrices(response, fileName);
        }
        else
        {
            break;
        }
    } while (dtFirst - dtFrom > 0.0001);

	if (recNum > 0)
		outFile.close();

    return true;
}

void printPrices(IO2GResponse *response, const char* fileName)
{
	bool saveToFile = (fileName[0] > 0);
	
	if (recNum == 0)
	{
		if (saveToFile)
			outFile.open(fileName, std::ios_base::out | std::ios_base::trunc);
		else
			std::cout << fileName << "...\n";
	}

    if (responseListener != 0)
    {
        if (response->getType() == MarketDataSnapshot)
        {
            //std::cout << "Request with RequestID='" << response->getRequestID() << "' is completed:" << std::endl;
            O2G2Ptr<IO2GResponseReaderFactory> factory = session->getResponseReaderFactory();
            if (factory)
            {
                O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = factory->createMarketDataSnapshotReader(response);
                if (reader)
                {
					if (saveToFile)
					{
						saveToFile = (outFile.is_open());
						if (recNum == 0)
							outFile << ("DateTime,BidOpen,BidHigh,BidLow,BidClose,AskOpen,AskHigh,AskLow,AskClose,Volume\n");
					}
					else
					{
						if (recNum == 0)
							std::cout << ("DateTime, BidOpen, BidHigh, BidLow, BidClose, AskOpen, AskHigh, AskLow, AskClose, Volume\n");
					}

                    char sTime[20];
					char buf[400];
					//std::string buf;
  				    
                    for (int ii = reader->size() - 1; ii >= 0; ii--)
                    {
                        DATE dt = reader->getDate(ii);
                        formatDate(dt, sTime);
                        if (reader->isBar())
                        {
							if (saveToFile)
							{
								sprintf(buf, "%s, %f, %f, %f, %f, %f, %f, %f, %f, %i\n",
											sTime, reader->getBidOpen(ii), reader->getBidHigh(ii), reader->getBidLow(ii), reader->getBidClose(ii),
											reader->getAskOpen(ii), reader->getAskHigh(ii), reader->getAskLow(ii), reader->getAskClose(ii), reader->getVolume(ii));
								outFile << buf ;
								std::cout <<  buf;
							}
							else
								std::cout <<  buf;

							recNum++;
                        }
                        else
                        {
							if (saveToFile)
							{
								sprintf(buf, "%s, %f", sTime, reader->getBid(ii), reader->getAsk(ii));
								outFile << buf << "\n";
							}
							else
								printf("DateTime=%s, Bid=%f, Ask=%f\n", sTime, reader->getBid(ii), reader->getAsk(ii));

							recNum++;
                        }
                    }
                }
            }
        }
    }
}
//-----------------------------------------------------------------------------------//
int __stdcall createMarketOrder(const char *sAccount, int iAmount, const char *sBuySell, const char *tif, const char *instrument, const char *ticketNum)
{
	int ret = 0;
	O2G2Ptr<IO2GOfferRow> offer = getOffer(session, instrument);
	char* accountID = getAccountID(session, sAccount);

	if (offer && accountID != NULL)
	{
		O2G2Ptr<IO2GRequest> request = createMarketOrderRequest(session, offer->getOfferID(), accountID, iAmount, sBuySell, tif, ticketNum);
		if (request)
        {
            responseListener->setRequestID(request->getRequestID());
            session->sendRequest(request);
            //if (responseListener->waitEvents())
            //{
                Sleep(2000); // Wait for the balance update
                std::cout << "Done createMarketOrder!" << std::endl;
				ret = 1;
            //}
            //else
            //{
            //    std::cout << "Response waiting timeout expired" << std::endl;
            //    ret = 0;
            //}
        }
	}
	return(ret);
}
//-----------------------------------------------------------------------------------//
int __stdcall createEntryOrder(const char *sAccount, int iAmount, const char *sBuySell, const char *tif, const char *instrument, const char* StopLimit, const double price)
{
	int ret = 0;
	O2G2Ptr<IO2GOfferRow> offer = getOffer(session, instrument);
	char* accountID = getAccountID(session, sAccount);

	if (offer && accountID != NULL)
	{
		O2G2Ptr<IO2GRequest> request = createEntryOrderRequest(session, offer->getOfferID(), accountID, iAmount, sBuySell, StopLimit, price);
		if (request)
        {
            responseListener->setRequestID(request->getRequestID());
            session->sendRequest(request);
            //if (responseListener->waitEvents())
            {
                Sleep(2000); // Wait for the balance update
                std::cout << "Done createEntryOrder!" << std::endl;
				ret = 1;
            }
            //else
            //{
            //    std::cout << "Response waiting timeout expired" << std::endl;
            //    ret = 0;
            //}
        }
	}
	return(ret);
}
//-----------------------------------------------------------------------------------//
int __stdcall createELSorder(const char *sAccount, int iAmount, const char *sBuySell, const char *instrument, const char *ordType1, const char *tif, const double price, const double limitPrice, const double stopPrice, const int trailStop)
{
	int ret = 0;
	char orderType[5];
	O2G2Ptr<IO2GOfferRow> offer = getOffer(session, instrument);
	char* accountID = getAccountID(session, sAccount);
	//char orderType2[4];
	//O2GRequestParamsEnum::TimeInForce;// tif1 = O2G2::TIF::GTC;


	if (offer && accountID != NULL)
	{
		if (price == 0 || ordType1[0] == 'M' || ordType1[0] == 'm')
			strcpy(orderType, "OM");
		else
		{
			if (ordType1[0] == 'S' || ordType1[0] == 's')
				strcpy(orderType, "SE");
			else if (ordType1[0] == 'L' || ordType1[0] == 'l')
					strcpy(orderType, "LE");
			else
				strcpy(orderType, "OM");
		}
			


		O2G2Ptr<IO2GRequest> request = createELSRequest(session, offer->getOfferID(), accountID, iAmount, price, limitPrice, stopPrice, sBuySell, orderType, tif, trailStop);
		if (request)
		{
			responseListener->setRequestID(request->getRequestID());
			session->sendRequest(request);
			//if (responseListener->waitEvents())
			{
				Sleep(2000); // Wait for the balance update
				std::cout << "Done createELSorder!" << std::endl;
				ret = 1;
			}
		}
	}
	return(ret);
}
//-----------------------------------------------------------------------------------//
IO2GRequest *createELSRequest(IO2GSession *session, const char *sOfferID, const char *sAccountID, int iAmount,	double dRate, double dRateLimit, double dRateStop, const char *sBuySell, const char *sOrderType, const char * tif, const int trailStop)
{
	O2G2Ptr<IO2GRequestFactory> requestFactory = session->getRequestFactory();
	if (!requestFactory)
	{
		std::cout << "Cannot create request factory" << std::endl;
		return NULL;
	}
	O2G2Ptr<IO2GValueMap> valuemap = requestFactory->createValueMap();
	valuemap->setString(Command, O2G2::Commands::CreateOrder);
	valuemap->setString(OrderType, sOrderType);
	valuemap->setString(AccountID, sAccountID);
	valuemap->setString(OfferID, sOfferID);
	valuemap->setString(BuySell, sBuySell);
	valuemap->setInt(Amount, iAmount);
	if (strcmp(sOrderType, "OM") != 0 && dRate > 0)
		valuemap->setDouble(Rate, dRate);
	valuemap->setDouble(RateLimit, dRateLimit);
	valuemap->setDouble(RateStop, dRateStop);
	valuemap->setString(TimeInForce, tif);
	valuemap->setString(CustomID, "ELS_order");

	if (trailStop > 0)
	{
		valuemap->setInt(TrailStepStop, trailStop);
	}

	if (tif > 0)
		valuemap->setString(TimeInForce, tif);

	O2G2Ptr<IO2GRequest> request = requestFactory->createOrderRequest(valuemap);
	if (!request)
	{
		std::cout << requestFactory->getLastError() << std::endl;
		return NULL;
	}
	return request.Detach();
}

//-----------------------------------------------------------------------------------//
IO2GRequest *createMarketOrderRequest(IO2GSession* session, const char* sOfferID, const char* sAccountID, int iAmount, const char* sBuySell, const char* tif, const char* ticketNum)
{
    O2G2Ptr<IO2GRequestFactory> requestFactory = session->getRequestFactory();
    if (!requestFactory)
    {
        std::cout << "Cannot create request factory" << std::endl;
        return NULL;
    }
    O2G2Ptr<IO2GValueMap> valuemap = requestFactory->createValueMap();
    valuemap->setString(Command, O2G2::Commands::CreateOrder);
    
    valuemap->setString(AccountID, sAccountID);
    valuemap->setString(OfferID, sOfferID);
    valuemap->setString(BuySell, sBuySell);
    valuemap->setInt(Amount, iAmount);
    valuemap->setString(CustomID, "MarketOrder");
	valuemap->setString(TimeInForce, tif); // O2G2::TIF::GTC);
	if (ticketNum == NULL || ticketNum[0] == 0)
		valuemap->setString(OrderType, O2G2::Orders::TrueMarketOpen);
	else
	{
		valuemap->setString(OrderType, O2G2::Orders::TrueMarketClose);
		valuemap->setString(TradeID, ticketNum);
	}
		

    O2G2Ptr<IO2GRequest> request = requestFactory->createOrderRequest(valuemap);
    if (!request)
    {
        std::cout << requestFactory->getLastError() << std::endl;
        return NULL;
    }
    return request.Detach();
}
//-----------------------------------------------------------------------------------//
IO2GRequest *createEntryOrderRequest(IO2GSession *session, const char *sOfferID, const char *sAccountID, int iAmount, const char *sBuySell, const char* StopLimit, const double price)
{
    O2G2Ptr<IO2GRequestFactory> requestFactory = session->getRequestFactory();
    if (!requestFactory)
    {
        std::cout << "Cannot create request factory" << std::endl;
        return NULL;
    }
    O2G2Ptr<IO2GValueMap> valuemap = requestFactory->createValueMap();
    valuemap->setString(Command, O2G2::Commands::CreateOrder);
	valuemap->setString(OrderType, StopLimit);
	valuemap->setDouble(Rate, price);
    valuemap->setString(AccountID, sAccountID);
    valuemap->setString(OfferID, sOfferID);
    valuemap->setString(BuySell, sBuySell);
    valuemap->setInt(Amount, iAmount);
    valuemap->setString(CustomID, "EntryOrder");
    O2G2Ptr<IO2GRequest> request = requestFactory->createOrderRequest(valuemap);
    if (!request)
    {
        std::cout << requestFactory->getLastError() << std::endl;
        return NULL;
    }
    return request.Detach();
}
//-----------------------------------------------------------------------------------//
int __stdcall getRealTimePrices(const char *instrument)
{
	responseListener->setInstrument(instrument);
	O2G2Ptr<IO2GLoginRules> loginRules = session->getLoginRules();
    if (loginRules)
    {
        O2G2Ptr<IO2GResponse> response = NULL;
        if (loginRules->isTableLoadedByDefault(Offers))
        {
            response = loginRules->getTableRefreshResponse(Offers);
            //if (response)
            //    responseListener->printOffers(session, response, "");
        }
        else
        {
            O2G2Ptr<IO2GRequestFactory> requestFactory = session->getRequestFactory();
            if (requestFactory)
            {
                O2G2Ptr<IO2GRequest> offersRequest = requestFactory->createRefreshTableRequest(Offers);
                responseListener->setRequestID(offersRequest->getRequestID());
                session->sendRequest(offersRequest);
                if (responseListener->waitEvents())
                {
                    response = responseListener->getResponse();
                    //if (response)
                    //    responseListener->printOffers(session, response, "");
                }
                else
                {
                    std::cout << "Response waiting timeout expired" << std::endl;
                    bWasError = true;
                }
            }
        }
        // Do nothing 10 seconds, let offers print
        Sleep(10000);
        std::cout << "Done getRealTimePrices!" << std::endl;
    }
	return(1);
}
//-----------------------------------------------------------------------------------//
int __stdcall stopPrices(const char *instrument)
{
	responseListener->setInstrument("");
	return(1);
}
//-----------------------------------------------------------------------------------//
__declspec(dllexport) void Register_Callback(fnCallBackFunc func)
{
	int count = 1;
	char buf[50];
 
	cb_func = func;

	// let's send 10 messages to the subscriber
	//while(count < 10)
	
	{
		// format the message
		sprintf(buf, "Message # %d", count); 

		// call the callback function
		cb_sendMsg(buf, 1);
		cb_func(buf, 11);
 
		count++;
 
		// Sleep for 2 seconds
		//Sleep(2000);
	}
	
}
//-----------------------------------------------------------------------------------//
//send FC API messages back to Matlab callback function
void cb_sendMsg(char *msg, double msgType)
{
	cb_func(msg, msgType);
}
//-----------------------------------------------------------------------------------//
char* getAccountID(IO2GSession *session, const char *sAccount)
{
	char s1[20], s2[20];
    O2G2Ptr<IO2GLoginRules> loginRules = session->getLoginRules();
    if (loginRules)
    {
        O2G2Ptr<IO2GResponse> response = loginRules->getTableRefreshResponse(Accounts);
        if (response)
        {
            O2G2Ptr<IO2GResponseReaderFactory> readerFactory = session->getResponseReaderFactory();
            if (readerFactory)
            {
                O2G2Ptr<IO2GAccountsTableResponseReader> reader = readerFactory->createAccountsTableReader(response);

                for (int i = 0; i < reader->size(); ++i)
                {
                    O2G2Ptr<IO2GAccountRow> account = reader->getRow(i);
                    if (account)
					{
						strcpy(s1, account->getAccountName());
						if (strcmp(s1, sAccount) == 0)
						{
							strcpy(s2, account->getAccountID());
							return s2;
						}
					}
                }
            }
        }
    }
    return NULL;
}


