fc_msg = 0;

if not(libisloaded('fc_matlab'))
    display('need login first');
else
    [fc_msg] = calllib('fc_matlab','createEntryOrder', '00997415', 6000, 'B', 'GTC', 'EUR/USD', 'LE', 1.09111);
end;    
%display(fc_msg);

%{
Parameters for createEntryOrder function:
accountID, trade size, side, TIF, symbol, order Type, price

account name - account name can be found in Trading Station
trade size - for FX min. 1000 or account min. trade size
side - B=Buy, S=Sell
Time in Force parameters: GTC, IOC, FOK 
symbol - any symbol available in your account
order type - LE, SE
price - limit or stop price

%}