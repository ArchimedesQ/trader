#ifndef BACKTESTENGINE_H
#define BACKTESTENGINE_H
#include <qobject.h>
#include <qstring.h>
#include <qstandarditemmodel.h>
#include <atomic>
#include <fstream>
#include "json11.h"
#include "CTAAPI.h"
#include "StrategyTemplate.h"
#include "eventengine.h"
struct VarData
{
	std::map<std::string, std::vector<BarData>>m_strategy_bardata;													//key�ǲ��� Value��K����
	std::map<std::string, std::map<std::string, std::vector<double>>>m_strategy_varplotrecord_mainchart;				//key�ǲ���Value��һ��map��key�Ǳ�������value��ֵ������
	std::map<std::string, std::map<std::string, std::vector<bool>>>m_strategy_varplotrecord_bool;					//key�ǲ���Value��һ��map��key�Ǳ�������value��ֵ������
	std::map<std::string, std::map<std::string, std::vector<std::string>>>m_strategy_varplotrecord_string;			//key�ǲ���Value��һ��map��key�Ǳ�������value��ֵ������
	std::map<std::string, std::map<std::string, std::vector<double>>>m_strategy_varplotrecord_indicator;				//key�ǲ���Value��һ��map��key�Ǳ�������value��ֵ������
	std::map<std::string, std::vector<double>>m_strategy_varplotrecord_pnl;											//��backtestengine�м�¼Bar����ӯ��
};

struct PLOTDATA
{
	std::map<std::string, std::vector<int>>m_capital_datetime;
	std::map<std::string, std::vector<double>>m_holding_and_totalwinning;						//��̬�Ӿ�̬ӯ��
	std::map<std::string, std::vector<double>>m_totalwinning;									//��̬��ӯ��
	std::map<std::string, std::vector<double>>m_Winning;										//ƽ����ӯ��
	std::map<std::string, std::vector<double>>m_Losing;										//ƽ���ܿ���
	std::map<std::string, std::vector<double>>m_drawdown;										//�س�
};

struct UnitResult
{
	double  totalwinning=0;
	double	maxCapital=0;
	double	drawdown=0;
	double	Winning=0;
	double	Losing=0;
	int totalResult=0;

	double holdingwinning=0;//�ֲ�ӯ��
	double holdingposition=0;
	double holdingprice=0;
};

class TradingResult
{
public:
	TradingResult(double entryPrice, std::string entryDt, double exitPrice, std::string exitDt, double volume, double rate, double slippage, double size);
	double m_entryPrice;  //���ּ۸�
	double m_exitPrice;   // ƽ�ּ۸�
	std::string m_entryDt;   // ����ʱ��datetime
	std::string  m_exitDt;  // ƽ��ʱ��
	double	m_volume;//���������� + / -������
	double	m_pnl;  //��ӯ��
	double m_turnover;
	double m_commission;
	double m_slippage;
};

typedef std::map<std::string, UnitResult> Result;//key�Ǻ�Լ value��һ�������λ

class BacktestEngine :public QObject, public CTAAPI
{
	Q_OBJECT
signals :
	void WriteLog(QString msg);
	void addItem(QString strategyname);
	void PlotCurve(PLOTDATA plotdata);
	void PlotVars(VarData vardata);
	void setrangesignal(int max);
	void setvaluesignal(int value);
public:
	BacktestEngine();
	~BacktestEngine();
	//�ṩ��Strategytemplate���õĽӿ�
	std::vector<std::string> sendOrder(std::string symbol, std::string orderType, double price, double volume, StrategyTemplate* Strategy);
	void cancelOrder(std::string orderID, std::string gatewayname);
	void writeCtaLog(std::string msg, std::string gatewayname);
	void PutEvent(std::shared_ptr<Event>e);
	std::vector<TickData> loadTick(std::string tickDbName, std::string symbol, int days);
	std::vector<BarData> loadBar(std::string BarDbName, std::string symbol, int days);
	//�ṩ������Ľӿ�
	void loadstrategy();											//���ز���
	void setStartDate(time_t datetime);
	void setStopDate(time_t datetime);
	void setMode(std::string mode);
	void GetTableViewData(QStandardItemModel &m_Model);
	void savetraderecord(std::string strategyname, std::shared_ptr<Event_Trade>etrade);
	std::string getparam(std::string strategyname, std::string param);
private:

	//progressbar value
	int m_progressbarvalue;

	//�ز�ı���
	std::atomic_int m_limitorderCount;						//�˹�ORDERID
	std::atomic_int  m_tradeCount;							//�ɽ�����
	std::string m_backtestmode;								//barģʽ����tickģʽ
	std::atomic<time_t> m_datetime;							//�Ա�ʱ��
	std::atomic_int working_worker; 	std::mutex m_workermutex;	//ͬ����
	std::condition_variable m_cv;
	std::map<std::string, double>symbol_size;						//��Լ����������Ӳ�̶�ȡ	ֻ����д�����ü���
	std::map<std::string, double>symbol_rate;						//��Լ����������Ӳ�̶�ȡ	ֻ����д�����ü���
	std::map<std::string, double>m_slippage;
	std::map<std::string, HINSTANCE>dllmap;																//��Ų���dll����		ֻ����д�����ü���
	std::map<std::string, std::shared_ptr<Event_Order>>m_Ordermap;			std::mutex m_ordermapmtx;	//���еĵ�������ɾ��
	std::map<std::string, std::shared_ptr<Event_Order>>m_WorkingOrdermap;								//�����еĵ�   ��workingordermap��һ����
	//key ��OrderID  value �ǲ��Զ��� ��;��Ϊ�˱�֤�������������Է���ȥ��  �ɽ��ر�����ֲ���ȷ���ض�Ӧ�Ĳ����ϣ��Է�������Խ���ͬһ����Լ����BUG
	std::map<std::string, StrategyTemplate*>m_orderStrategymap;	            std::mutex m_orderStrategymtx;
	std::map<std::string, StrategyTemplate*>m_strategymap;					std::mutex m_strategymtx;	//��������Ͳ��Զ���ָ��
	//key ��Լ����value ����ָ��(ʵ��)vector  �����Ѳ�ͬ�ĺ�Լtick���͵�ÿһ�����Զ���
	std::map<std::string, std::vector<StrategyTemplate*>>m_tickstrategymap;	std::mutex m_tickstrategymtx;
	
	std::set<std::string>m_backtestsymbols;									std::mutex	m_backtestsymbolmtx;//Ҫ�ز�ĺ�Լ�б� ������ȡ����
	std::vector<TickData>m_Tickdatavector;									std::mutex m_HistoryDatamtx;//��ʷ���ݼ���
	std::vector<BarData>m_Bardatavector;

	std::map<std::string, std::map<std::string, std::vector<Event_Trade>>>m_tradelist_memory;					std::mutex m_tradelistmtx;//���㻺��
	std::map<std::string, Result>m_result;											std::mutex m_resultmtx;			//������
	PLOTDATA m_plot_data;													std::mutex m_plotmtx;//��ͼ����
	//�ز�ʱ��
	time_t startDatetime;
	time_t endDatetime;
	//�ز⺯��
	void LoadHistoryData(std::string collectionstring);				//��ȡ�ز��õ���ʷ����
	void processTickEvent(std::shared_ptr<Event>e);					//����tick�¼�
	void processBarEvent(std::shared_ptr<Event>e);					//����bar�¼�
	void CrossLimitOrder(const TickData&data);
	void CrossLimitOrder(const BarData&data);
	void Settlement(std::shared_ptr<Event_Trade>etrade, std::map<std::string, StrategyTemplate*>orderStrategymap);
	void RecordCapital(const TickData &data);
	void RecordCapital(const BarData &data);
	void RecordPNL(const TickData &data);
	void RecordPNL(const BarData &data);


	//�ز�����_(:�١���)_
	std::mutex m_varplotrecordmtx;
	void VarPlotRecord(StrategyTemplate* strategy_ptr,const BarData& bardata);
	VarData m_Vardata;

	//�¼�����
	EventEngine *m_eventengine;
	void RegEvent();

	//MongoDB
	mongoc_uri_t         *m_uri;

	//�ַ���ת��
	QString str2qstr(const std::string str);
	//ʱ��ת��
	std::string time_t2str(time_t datetime);
	private slots:
	//
	void Runbacktest();
	void Stopbacktest();
};
#endif