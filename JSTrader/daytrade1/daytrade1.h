#ifndef DAYTRADE1_H
#define DAYTRADE1_H
#ifdef  DAYTRADE1_EXPORTS
#define DAYTRADE1_API __declspec(dllexport)
#else
#define DAYTRADE1_API __declspec(dllimport)
#endif
#include"StrategyTemplate.h"
extern "C" DAYTRADE1_API StrategyTemplate * CreateStrategy(CTAAPI *ctamanager);//��������
extern "C" DAYTRADE1_API int ReleaseStrategy();//�ͷŲ���
std::vector<StrategyTemplate*>g_DAYTRADE1_v;//ȫ��ָ��

#include<vector>
#include<algorithm>
#include<regex>
#include"talib.h"
#include"libbson-1.0\bson.h"
#include"libmongoc-1.0\mongoc.h"
class DAYTRADE1_API Daytrade1 : public StrategyTemplate
{
public:
	Daytrade1(CTAAPI *ctamanager);
	~Daytrade1();

	void onInit();
	//TICK
	void onTick(TickData Tick);
	//BAR
	void onBar(BarData Bar);

	//�����ص�
	void onOrder(std::shared_ptr<Event_Order>e);
	//�ɽ��ص�
	void onTrade(std::shared_ptr<Event_Trade>e);

	virtual void savepostomongo();
	virtual void loadposfrommongo();

	//����
	double unit_volume;


	//ָ��
	std::mutex m_HLCmtx;
	std::vector<double>close_vector;
	std::vector<double>high_vector;
	std::vector<double>low_vector;


	double TodayOpen;
	 
	double EntryPrice;

	double buysellsignal;

	double stopsignal;

	const double range = 5;
	const double stoploss = 15;


	std::set<std::string> m_ninetoeleven;
	std::set<std::string> m_ninetohalfeleven;
	std::set<std::string> m_ninetoone;
	std::set<std::string> m_ninetohalftwo;
	BarData m_5Bar;
	//������
	double lastprice;				//���½���
	//���½���
	void putEvent();
};
#endif