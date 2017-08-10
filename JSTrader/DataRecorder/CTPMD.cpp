#include"CTPMD.h"
#include"eventengine.h"
#include<fstream>
#include<iostream>

CTPMD::CTPMD(EventEngine *eventengine, std::string gatewayname) :JSRecordClass(eventengine, "CTP")
{
	m_gatewayname = gatewayname;
	m_ptr_subscribedSymbols = new std::set<std::string>;
	m_ptr_mdapi = NULL;
}

CTPMD::~CTPMD()
{
	delete m_ptr_subscribedSymbols;
}

void CTPMD::OnFrontConnected()
{
	//�������ӳɹ�
	m_connectionStatus = true;
	std::shared_ptr<Event_Log>e(new Event_Log);
	e->msg = "������������ӳɹ�";
	e->gatewayname = m_gatewayname;
	onLog(e);
	login();
}
void CTPMD::OnFrontDisconnected(int nReason)
{
	//�������ӶϿ�
	m_connectionStatus = false;
	m_loginStatus = false;
	std::shared_ptr<Event_Log>e = std::make_shared<Event_Log>();
	e->gatewayname = m_gatewayname;
	e->msg = "������������ӶϿ�";
	onLog(e);
}

void CTPMD::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	std::shared_ptr<Event_Error>e = std::make_shared<Event_Error>();
	e->errorMsg = pRspInfo->ErrorMsg;
	e->errorID = pRspInfo->ErrorID;
	e->gatewayname = m_gatewayname;
	onError(e);
}

void CTPMD::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//��¼�ر�
	if (!IsErrorRspInfo(pRspInfo))
	{
		//��¼�ɹ�
		m_loginStatus = true;
		std::shared_ptr<Event_Log>e = std::make_shared<Event_Log>();
		e->gatewayname = m_gatewayname;
		e->msg = "�����������¼���";
		onLog(e);
	}
	else
	{
		std::shared_ptr<Event_Error>e = std::make_shared<Event_Error>();
		e->errorMsg = pRspInfo->ErrorMsg;
		e->errorID = pRspInfo->ErrorID;
		e->gatewayname = m_gatewayname;
		onError(e);
	}
}

void CTPMD::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//�ǳ��ر�
	if (!IsErrorRspInfo(pRspInfo))
	{
		m_loginStatus = false;
		std::shared_ptr<Event_Log>e = std::make_shared<Event_Log>();
		e->gatewayname = m_gatewayname;
		e->msg = "����������ǳ����";
		onLog(e);
	}
	else
	{
		std::shared_ptr<Event_Error>e = std::make_shared<Event_Error>();
		e->errorMsg = pRspInfo->ErrorMsg;
		e->errorID = pRspInfo->ErrorID;
		e->gatewayname = m_gatewayname;
		onError(e);
	}
}


void CTPMD::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
	std::shared_ptr<Event_Tick>e = std::make_shared<Event_Tick>();
	e->gatewayname = m_gatewayname;
	e->symbol = pDepthMarketData->InstrumentID;
	e->exchange = pDepthMarketData->ExchangeID;
	e->lastprice = pDepthMarketData->LastPrice;
	e->volume = pDepthMarketData->Volume;
	e->openInterest = pDepthMarketData->OpenInterest;
	e->time = std::string(pDepthMarketData->UpdateTime) + "." + std::to_string(pDepthMarketData->UpdateMillisec / 100);
	e->date = pDepthMarketData->TradingDay;

	e->openPrice = pDepthMarketData->OpenPrice;
	e->highPrice = pDepthMarketData->HighestPrice;
	e->lowPrice = pDepthMarketData->LowestPrice;
	e->preClosePrice = pDepthMarketData->PreClosePrice;
	e->upperLimit = pDepthMarketData->UpperLimitPrice;
	e->lowerLimit = pDepthMarketData->LowerLimitPrice;
	e->bidprice1 = pDepthMarketData->BidPrice1;
	e->askprice1 = pDepthMarketData->AskPrice1;
	e->bidvolume1 = pDepthMarketData->BidVolume1;
	e->askvolume1 = pDepthMarketData->AskVolume1;
	onTick(e);

}

bool CTPMD::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// ���ErrorID != 0, ˵���յ��˴������Ӧ
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	return bResult;
}
void CTPMD::connect()
{

	//��������ͽ���
	//��ɨ���ĵ��µ��˺����������ļ�
	if (_access("./CTPGateway", 0) != -1)
	{
		std::fstream f;
		f.open("./CTPGateway/CTP_connect");
		if (!f.is_open())
		{
			//����򲻿��ļ�
			std::shared_ptr<Event_Log>e = std::make_shared<Event_Log>();
			e->msg = "�޷���ȡ���������ļ�";
			e->gatewayname = m_gatewayname;
			this->onLog(e);
			return;
		}
		std::string line;
		std::map<std::string, std::string>configmap;
		while (!f.eof())
		{
			getline(f, line);
			std::string::size_type pos = line.find("=");//���յȺŷָ�
			configmap.insert(std::make_pair(line.substr(0, pos), line.substr(pos + 1, line.size() - 1)));
		}
		connectmd(configmap["userid"], configmap["password"], configmap["brokerid"], configmap["mdaddress"]);
		//initQuery();
	}
	else
	{
		std::shared_ptr<Event_Log>e = std::make_shared<Event_Log>();
		e->msg = "�޷���ȡ���������ļ�";
		e->gatewayname = m_gatewayname;
		this->onLog(e);
	}
}
void CTPMD::connectmd(std::string userID, std::string password, std::string brokerID, std::string address)
{
	//����
	m_userID = userID;
	m_password = password;
	m_brokerID = brokerID;
	m_address = address;
	//
	if (m_connectionStatus == false)
	{
		m_connectionStatus = true;
		if (_access("./temp/", 0) == -1)
		{
			_mkdir("./temp/");
			//���������Ŀ¼��Ҫ����
		}
		m_ptr_mdapi = CThostFtdcMdApi::CreateFtdcMdApi("./temp/CTPmd");
		m_ptr_mdapi->RegisterSpi(this);
		m_ptr_mdapi->RegisterFront((char*)m_address.c_str());
		m_ptr_mdapi->Init();
	}
	else
	{
		if (m_loginStatus == true)
		{
			login();
		}
	}
}

void CTPMD::subscribe(SubscribeReq& subscribeReq)
{
	if (m_loginStatus == true)
	{
		subscribeMarketData(subscribeReq.symbol);
	}
	m_ptr_subscribedSymbols->insert(subscribeReq.symbol);
}

void CTPMD::subscribeMarketData(std::string instrumentID)
{
	char* buffer = (char*)instrumentID.c_str();
	char* myreq[1] = { buffer };
	m_ptr_mdapi->SubscribeMarketData(myreq, 1);
}

void CTPMD::login()
{
	//��¼
	if (m_userID.empty() == false && m_password.empty() == false && m_brokerID.empty() == false)
	{
		CThostFtdcReqUserLoginField myreq;
		strncpy(myreq.BrokerID, m_brokerID.c_str(), sizeof(myreq.BrokerID) - 1);
		strncpy(myreq.UserID, m_userID.c_str(), sizeof(myreq.UserID) - 1);
		strncpy(myreq.Password, m_password.c_str(), sizeof(myreq.Password) - 1);
		m_reqID++;
		m_ptr_mdapi->ReqUserLogin(&myreq, m_reqID);//��¼�˺�
	}
}

void CTPMD::logout()
{
	CThostFtdcUserLogoutField myreq;
	strncpy(myreq.BrokerID, m_brokerID.c_str(), sizeof(myreq.BrokerID) - 1);
	strncpy(myreq.UserID, m_userID.c_str(), sizeof(myreq.UserID) - 1);
	m_reqID++;
	int i=m_ptr_mdapi->ReqUserLogout(&myreq,m_reqID);
}

void CTPMD::close()
{
	if (m_ptr_mdapi != NULL)
	{
		logout();
		m_connectionStatus = false;
		m_loginStatus = false;
		m_ptr_mdapi->RegisterSpi(NULL);
		m_ptr_mdapi->Release();
		m_ptr_mdapi = NULL;
	}
}
