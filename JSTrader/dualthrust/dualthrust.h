#ifndef DUALTHRUST_H
#define DUALTHRUST_H
#ifdef  DUALTHRUST_EXPORTS
#define DUALTHRUST_API __declspec(dllexport)
#else
#define DUALTHRUST_API __declspec(dllimport)
#endif
#include"StrategyTemplate.h"
extern "C" DUALTHRUST_API StrategyTemplate * CreateStrategy(CTAAPI *ctamanager);//��������
extern "C" DUALTHRUST_API int ReleaseStrategy();//�ͷŲ���
std::vector<StrategyTemplate*>g_DUALTHRUST_v;//ȫ��ָ��

#include<vector>
#include<algorithm>
#include<regex>
#include"talib.h"
#include"libbson-1.0\bson.h"
#include"libmongoc-1.0\mongoc.h"
class DUALTHRUST_API dualthrust : public StrategyTemplate
{
public:
	dualthrust(CTAAPI *ctamanager);
	~dualthrust();

	void onInit();
	//TICK
	void onTick(TickData Tick);
	//BAR
	void onBar(BarData Bar);
	void on5Bar(BarData Bar);
	void ondailyBar(BarData Bar);
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
	std::vector<double>close_daily; 
	double lastHour;
	double MA100;
	double lots = 1;
	double TimesMaxToday = 1;
	double K1 = 0.7;
	double K2 = 0.7;
	double Mday = 1;
	double Nday = 1;

	double BuyRange = 0;
	double SellRange = 0;
	double BuyTrig= 0;
	double SellTrig = 0;
	double HH;
	double LL;
	double HC;
	double LC;
	double SellPosition;
	double BuyPosition;
	double TimesToday;


	double Bars=0;
	std::set<std::string> m_ninetoeleven;
	std::set<std::string> m_ninetohalfeleven;
	std::set<std::string> m_ninetoone;
	std::set<std::string> m_ninetohalftwo;
	BarData m_5Bar;
	BarData m_dailyBar;
	//������
	double lastprice;				//���½���
	//���½���
	void putEvent();
};
#endif