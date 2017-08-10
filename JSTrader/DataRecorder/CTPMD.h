#ifndef __MD_H__
#define __MD_H__
#include<string>
#include<set>
#include"ThostFtdcMdApi.h"
#include<direct.h>
#include<io.h>
#include"structs.h"
#include"eventengine.h"
#include"JSRecordClass.h"
class CTPMD :public CThostFtdcMdSpi,public JSRecordClass
{
public:
	CTPMD(EventEngine *eventengine, std::string gatewayname);
	~CTPMD();
	///���ͻ����뽻�׺�̨������ͨ������ʱ����δ��¼ǰ�����÷��������á�
	virtual void OnFrontConnected();

	///���ͻ����뽻�׺�̨ͨ�����ӶϿ�ʱ���÷��������á���������������API���Զ��������ӣ��ͻ��˿ɲ�������
	///@param nReason ����ԭ��
	///        0x1001 �����ʧ��
	///        0x1002 ����дʧ��
	///        0x2001 ����������ʱ
	///        0x2002 ��������ʧ��
	///        0x2003 �յ�������
	virtual void OnFrontDisconnected(int nReason);

	///��¼������Ӧ
	virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///�ǳ�������Ӧ
	virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///����Ӧ��
	virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///�������֪ͨ
	virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData);

	//�жϴ���ĺ���
	bool IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo);
	//////////////////��������

	void connect();

    void connectmd(std::string userID, std::string password, std::string brokerID, std::string address);
	//��¼����
	virtual void login();
	//�����¼
	virtual void subscribe(SubscribeReq& subscribeReq);

	virtual void logout();

	void subscribeMarketData(std::string instrumentID);
	//����
	virtual void close();
	//�˳��߳�
private:
	CThostFtdcMdApi* m_ptr_mdapi;
	std::string m_gatewayname;
	int m_reqID;
	bool m_connectionStatus = false;
	bool m_loginStatus = false;
	std::set<std::string>m_subscribedSymbols;
	std::string m_userID;//�˺����� �����̵�ַ����9999 1080 ��������ַtcp://xxx.xxx.xxx.xxx
	std::string m_password;
	std::string m_brokerID;
	std::string m_address;
	std::set<std::string> *m_ptr_subscribedSymbols = nullptr;
};
#endif