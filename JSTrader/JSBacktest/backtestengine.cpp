#include "backtestengine.h"
#include "deleterecord.hpp"
typedef StrategyTemplate*(*Dllfun)(CTAAPI*);
typedef int(*Release)();

BacktestEngine::BacktestEngine()
{
	m_eventengine = new EventEngine();
	RegEvent();
	m_eventengine->StartEngine();
	//��ʼ��MONGODB
	mongoc_init();													//1

	m_uri = mongoc_uri_new("mongodb://localhost:27017/");			//2
	// �����ͻ��˳�
	m_pool = mongoc_client_pool_new(m_uri);							//3

	//��ȡsymbol size
	std::fstream f;
	f.open("./size.csv");
	std::string line;
	std::vector<std::string>v;
	while (!f.eof())
	{
		std::getline(f, line);
		v = Utils::split(line, ",");
		if (v.size() == 2)
		{
			symbol_size.insert(std::pair<std::string, double>(v[0], atof(v[1].c_str())));
		}
	}
	f.close();
}

BacktestEngine::~BacktestEngine()
{
	delete m_eventengine;
	mongoc_client_pool_destroy(m_pool);
	mongoc_uri_destroy(m_uri);
	mongoc_cleanup();
	for (std::map<std::string, HINSTANCE>::iterator it = dllmap.begin(); it != dllmap.end(); it++)
	{
		Release result = (Release)GetProcAddress(it->second, "ReleaseStrategy");//��������
		result();
	}
}

QString BacktestEngine::str2qstr(const std::string str)
{
	return QString::fromLocal8Bit(str.data());
}

std::string BacktestEngine::time_t2str(time_t datetime)
{
	auto tt = datetime;
	struct tm* ptm = localtime(&tt);
	char date[60] = { 0 };
	sprintf(date, "%d-%02d-%02d      %02d:%02d:%02d",
		(int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
		(int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
	return std::string(date);
}

//ע���¼�
void BacktestEngine::RegEvent()
{
	m_eventengine->RegEvent(EVENT_BACKTEST_TICK, std::bind(&BacktestEngine::processTickEvent, this, std::placeholders::_1));
	m_eventengine->RegEvent(EVENT_BACKTEST_BAR, std::bind(&BacktestEngine::processBarEvent, this, std::placeholders::_1));
}
//�����ز��ֹͣ�ز�

void BacktestEngine::Runbacktest()
{
	if (m_backtestsymbols.empty())
	{
		writeCtaLog("û����Ӳ�����ز��ë�߰� (�p�����)����(����)3��)","");
		return;
	}
	Stopbacktest();//�����һ�λ���
	deletedir("./traderecord/*");

	m_limitorderCount = 0;
	m_tradeCount = 0;
	working_worker = 0;
	m_progressbarvalue = 1;
	//��һ����ȡ��ʷ����
	writeCtaLog(">>>��ȡ��ʷ����", "BacktestEngine");
	std::unique_lock<std::mutex>lck(m_backtestsymbolmtx);
	for (std::set<std::string>::iterator it = m_backtestsymbols.begin(); it != m_backtestsymbols.end(); it++)
	{
		LoadHistoryData((*it));
	}
	lck.unlock();
	writeCtaLog(">>>��ȡ��ʷ�������", "BacktestEngine");
	writeCtaLog(">>>��ʼ����������", "BacktestEngine");
	std::unique_lock<std::mutex>lck2(m_tickstrategymtx);
	for (std::map<std::string, std::vector<StrategyTemplate*>>::iterator iter = m_tickstrategymap.begin(); iter != m_tickstrategymap.end(); iter++)
	{
		for (std::vector<StrategyTemplate*>::iterator it = iter->second.begin(); it != iter->second.end(); it++)
		{
			(*it)->TradingMode = BacktestMode;
			(*it)->onInit();
			std::vector<std::string>symbollist=Utils::split((*it)->getparam("symbol"), ",");
			for(std::vector<std::string>::iterator iterator = symbollist.begin(); iterator != symbollist.end();iterator++)
			{
				symbol_rate[Utils::regMySymbol(*iterator)] = atof((*it)->getparam(*iterator).c_str());
				if ((*it)->getparam("slippage_" + Utils::regMySymbol(*iterator)) != "Null")
				{
					m_slippage[Utils::regMySymbol(*iterator)] = atof((*it)->getparam("slippage_" + Utils::regMySymbol(*iterator)).c_str());
				}
				else
				{
					m_slippage[Utils::regMySymbol(*iterator)] = 0;
				}
			}
		}
	}
	writeCtaLog(">>>��ʼ�������������", "BacktestEngine");
	writeCtaLog(">>>������������", "BacktestEngine");
	for (std::map<std::string, std::vector<StrategyTemplate*>>::iterator iter = m_tickstrategymap.begin(); iter != m_tickstrategymap.end(); iter++)
	{
		for (std::vector<StrategyTemplate*>::iterator it = iter->second.begin(); it != iter->second.end(); it++)
		{
			(*it)->trading = true;
			(*it)->onStart();
		}
	}
	lck2.unlock();
	writeCtaLog(">>>���������������", "BacktestEngine");

	for (std::map<std::string, std::vector<StrategyTemplate*>>::iterator iter = m_tickstrategymap.begin(); iter != m_tickstrategymap.end(); iter++)
	{
		for (std::vector<StrategyTemplate*>::iterator it = iter->second.begin(); it != iter->second.end(); it++)
		{
			Result result;
			std::vector<std::string> a = Utils::split((*it)->getparam("symbol"), ",");
			for (std::vector<std::string>::iterator itit = a.begin(); itit != a.end(); itit++)
			{
				if (*itit != "")
				{
					UnitResult unitresult;
					result.insert(std::pair<std::string, UnitResult>(*itit, unitresult));
				}
			}
			m_result.insert(std::pair<std::string, Result>((*it)->getparam("name"), result));
		}
	}
	writeCtaLog(">>>result�洢�����������", "BacktestEngine");

	writeCtaLog(">>>��ʼ�ط�����", "BacktestEngine");
	emit setrangesignal(m_Bardatavector.size());
	std::unique_lock<std::mutex>lck3(m_HistoryDatamtx);
	if (m_backtestmode == BAR_MODE)
	{
		//����
		std::sort(m_Bardatavector.begin(), m_Bardatavector.end(), BarGreater());
		if (m_Bardatavector.empty())
		{
			writeCtaLog("ľ��������ز��ë����~","");
			return;
		}
		WriteLog(str2qstr("��һ�����ݵ�ʱ����:" + m_Bardatavector.front().date + m_Bardatavector.front().time));
		WriteLog(str2qstr("���һ�����ݵ�ʱ����:" + m_Bardatavector.back().date + m_Bardatavector.back().time));
		for (std::vector<BarData>::iterator iter = m_Bardatavector.begin(); iter != m_Bardatavector.end(); iter++)
		{
			if (iter == m_Bardatavector.begin())
			{
				m_datetime = (*m_Bardatavector.begin()).unixdatetime;
			}
			//�¼�����д��
			std::shared_ptr<Event_Backtest_Bar>eBar = std::make_shared<Event_Backtest_Bar>();
			eBar->bar = (*iter);
			std::unique_lock<std::mutex>lck(m_workermutex);
			while (((*iter).unixdatetime) != m_datetime)
			{
				if (working_worker != 0)
				{
					m_cv.wait(lck);
				}
				m_datetime = ((*iter).unixdatetime);
				emit setvaluesignal(m_progressbarvalue);
			}
			working_worker = working_worker + 1;
			m_eventengine->Put(eBar);
			m_progressbarvalue++;
		}
		emit setvaluesignal(m_progressbarvalue);
	}
	else if (m_backtestmode == TICK_MODE)
	{
		//����
		std::sort(m_Tickdatavector.begin(), m_Tickdatavector.end(), TickGreater());
		for (std::vector<TickData>::iterator iter = m_Tickdatavector.begin(); iter != m_Tickdatavector.end(); iter++)
		{
			if (iter == m_Tickdatavector.begin())
			{
				m_datetime = (*m_Tickdatavector.begin()).unixdatetime;
			}
			//�¼�����д��
			std::shared_ptr<Event_Backtest_Tick>eTick = std::make_shared<Event_Backtest_Tick>();
			eTick->tick = (*iter);
			std::unique_lock<std::mutex>lck(m_workermutex);
			while (((*iter).unixdatetime) != m_datetime)
			{
				if (working_worker != 0)
				{
					m_cv.wait(lck);
				}
				m_datetime = ((*iter).unixdatetime);
				emit setvaluesignal(m_progressbarvalue);
			}
			working_worker = working_worker + 1;
			m_eventengine->Put(eTick);
			m_progressbarvalue++;
		}
		emit setvaluesignal(m_progressbarvalue);
	}
	else
	{
		writeCtaLog(">>>û������mode", "BacktestEngine");
		return;
	}
	writeCtaLog(">>>�ز����", "BacktestEngine");

	while (true)
	{
		if (working_worker == 0)
		{
			break;
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	emit PlotCurve(m_plot_data);
	emit PlotVars(m_Vardata);
}

void BacktestEngine::Stopbacktest()
{
	std::unique_lock<std::mutex>lck(m_HistoryDatamtx);
	m_Bardatavector.clear();
	m_Tickdatavector.clear();
	std::unique_lock<std::mutex>lck1(m_ordermapmtx);
	m_Ordermap.clear();
	m_WorkingOrdermap.clear();
	std::unique_lock<std::mutex>lck2(m_orderStrategymtx);
	m_orderStrategymap.clear();
	std::unique_lock<std::mutex>lck3(m_tradelistmtx);
	m_tradelist_memory.clear();
	std::unique_lock<std::mutex>lck4(m_resultmtx);
	m_result.clear();
	std::unique_lock<std::mutex>lck5(m_plotmtx);
	m_plot_data.m_capital_datetime.clear();
	m_plot_data.m_drawdown.clear();
	m_plot_data.m_holding_and_totalwinning.clear();
	m_plot_data.m_Losing.clear();
	m_plot_data.m_totalwinning.clear();
	m_plot_data.m_Winning.clear();

	m_Vardata.m_strategy_bardata.clear();
	m_Vardata.m_strategy_varplotrecord_bool.clear();
	m_Vardata.m_strategy_varplotrecord_pnl.clear();
	m_Vardata.m_strategy_varplotrecord_mainchart.clear();
	m_Vardata.m_strategy_varplotrecord_string.clear();
	m_Vardata.m_strategy_varplotrecord_indicator.clear();
}

//���ز���
void BacktestEngine::loadstrategy()
{
	//���ļ��е��»�ȡ���в����ļ� ��windowsAPI����DLL
	if (_access("./Strategy", 0) != -1)
	{
		std::filebuf in;
		if (!in.open("./Strategy/strategysetting.json", std::ios::in))
		{
			emit WriteLog(str2qstr("�޷���ȡ�����ļ�"));
			return;
		}
		std::istream iss(&in);
		std::istreambuf_iterator<char> eos;
		std::string s(std::istreambuf_iterator<char>(iss), eos);
		std::string err;

		auto json = json11::Json::parse(s, err);
		if (!err.empty()) {
			in.close();
			return;
		}

		json11::Json::array jsonarray = json.array_items();
		for (int i = 0; i < jsonarray.size(); i++)
		{
			std::string strategy = "./Strategy/" + jsonarray[i]["strategy"].string_value();
			std::vector<std::string>symbol_v;//��ʱ�洢�����ļ����׵ĺ�Լ
			json11::Json::array array_symbol = jsonarray[i]["symbol"].array_items();
			for (int j = 0; j < array_symbol.size(); j++)
			{
				//����Լ�б����symbol_v��
				symbol_v.push_back(array_symbol[j].string_value());
			}
			json11::Json::array array_ = jsonarray[i]["param"].array_items();
			std::map<std::string, std::string>tmpparammap;
			for (int j = 0; j < array_.size(); j++)
			{
				//��������
				json11::Json::object obj = array_[j].object_items();
				for (json11::Json::object::iterator it = obj.begin(); it != obj.end(); it++)
				{
					auto t = it->first;//����
					auto tt = it->second;//ֵ
					tmpparammap.insert(std::pair<std::string, std::string>(t, tt.string_value()));
				}
				std::string symbol = "";//��ʱ��symbol
				for (std::vector<std::string>::iterator iter = symbol_v.begin(); iter != symbol_v.end(); iter++)
				{
					symbol += (*iter + ",");
				}
				tmpparammap.insert(std::pair<std::string, std::string>("symbol", symbol));
			}

			//���������
			HINSTANCE his = LoadLibraryA(strategy.c_str());//����һ������
			if (his == NULL)
			{
				//û�м��ؽ���DLL
				emit WriteLog(str2qstr("����DLL��ʧ!"));
				break;
			}
			Dllfun dll = (Dllfun)GetProcAddress(his, "CreateStrategy");//��������
			if (dll == NULL)
			{
				//û�м��ؽ���DLL
				emit WriteLog(str2qstr("�޷�����CreateStrategy����"));
				FreeLibrary(his);
				break;
			}
			StrategyTemplate* strategy_ptr = dll(this);
			//��ֵ����
			for (std::map<std::string, std::string>::iterator it = tmpparammap.begin(); it != tmpparammap.end(); it++)
			{
				//����parameter
				strategy_ptr->checkparam(it->first.c_str(), it->second.c_str());
			}
			//����pos_map

			for (std::vector<std::string>::iterator iter = symbol_v.begin(); iter != symbol_v.end(); iter++)
			{
				strategy_ptr->checkSymbol((*iter).c_str());

			}
			if (strategy_ptr->getparam("name") == "Null")
			{
				this->writeCtaLog("��������һ��û�и�����������", strategy_ptr->gatewayname);
			}
			else
			{
				strategy_ptr->putEvent();
				strategy_ptr->putGUI();
			}
			m_strategymap.insert(std::pair<std::string, StrategyTemplate*>(strategy_ptr->getparam("name"), strategy_ptr));//�������Ͳ���
			dllmap.insert(std::pair<std::string, HINSTANCE>(strategy_ptr->getparam("name"), his));//������

			emit addItem(str2qstr(strategy_ptr->getparam("name")));
		}
	}
	else
	{
		emit WriteLog("û��Ŀ¼");
	}
}

//�ز���غ���
void BacktestEngine::LoadHistoryData(std::string collectionstring)
{
	std::unique_lock<std::mutex>lck(m_HistoryDatamtx);
	const char* databasename;
	if (m_backtestmode == TICK_MODE)
	{
		databasename = "CTPTickDb";
		const char* collectionsname = collectionstring.c_str();

		mongoc_cursor_t *cursor;
		bson_error_t error;
		const bson_t *doc;
		mongoc_client_t    *client = mongoc_client_pool_pop(m_pool);																				//ȡһ��mongo����

		bson_t parent;
		bson_t child;
		mongoc_collection_t *collection;

		bson_init(&parent);
		BSON_APPEND_DOCUMENT_BEGIN(&parent, "datetime", &child);
		BSON_APPEND_TIME_T(&child, "$gt", startDatetime);
		BSON_APPEND_TIME_T(&child, "$lt", endDatetime);
		bson_append_document_end(&parent, &child);


		char * str = bson_as_json(&parent, NULL);
		//	printf("\n%s\n", str);

		collection = mongoc_client_get_collection(client, databasename, collectionsname);

		cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 0, 0, &parent, NULL, NULL);

		while (mongoc_cursor_next(cursor, &doc)) {
			str = bson_as_json(doc, NULL);
			std::string s = str;
			std::string err;


			auto json = json11::Json::parse(s, err);
			if (!err.empty())
			{
				mongoc_cursor_destroy(cursor);
				mongoc_client_pool_push(m_pool, client);																						//�Ż�һ��mongo����
				return;
			}
			TickData tickdata;

			tickdata.symbol = json["symbol"].string_value();
			tickdata.exchange = json["exchange"].string_value();
			tickdata.gatewayname = json["gatewayname"].string_value();

			tickdata.lastprice = json["lastprice"].number_value();
			tickdata.volume = json["volume"].number_value();
			tickdata.openInterest = json["openInterest"].number_value();

			json11::Json::object datetime = json["datetime"].object_items();
			tickdata.unixdatetime = datetime["$date"].number_value() / 1000;

			tickdata.date = json["date"].string_value();
			tickdata.time = json["time"].string_value();

			tickdata.openPrice = json["openPrice"].number_value();//���տ�
			tickdata.highPrice = json["highPrice"].number_value();//���ո�
			tickdata.lowPrice = json["lowPrice"].number_value();//���յ�
			tickdata.preClosePrice = json["preClosePrice"].number_value();//����

			tickdata.upperLimit = json["upperLimit"].number_value();//��ͣ
			tickdata.lowerLimit = json["lowerLimit"].number_value();//��ͣ

			tickdata.bidprice1 = json["bidprice1"].number_value();
			tickdata.bidprice2 = json["bidprice2"].number_value();
			tickdata.bidprice3 = json["bidprice3"].number_value();
			tickdata.bidprice4 = json["bidprice4"].number_value();
			tickdata.bidprice5 = json["bidprice5"].number_value();

			tickdata.askprice1 = json["askprice1"].number_value();
			tickdata.askprice2 = json["askprice2"].number_value();
			tickdata.askprice3 = json["askprice3"].number_value();
			tickdata.askprice4 = json["askprice4"].number_value();
			tickdata.askprice5 = json["askprice5"].number_value();

			tickdata.bidvolume1 = json["bidvolume1"].number_value();
			tickdata.bidvolume2 = json["bidvolume2"].number_value();
			tickdata.bidvolume3 = json["bidvolume3"].number_value();
			tickdata.bidvolume4 = json["bidvolume4"].number_value();
			tickdata.bidvolume5 = json["bidvolume5"].number_value();

			tickdata.askvolume1 = json["askvolume1"].number_value();
			tickdata.askvolume2 = json["askvolume2"].number_value();
			tickdata.askvolume3 = json["askvolume3"].number_value();
			tickdata.askvolume4 = json["askvolume4"].number_value();
			tickdata.askvolume5 = json["askvolume5"].number_value();

			m_Tickdatavector.push_back(tickdata);

			//		printf("%s\n", str);
			bson_free(str);
		}

		if (mongoc_cursor_error(cursor, &error)) {
			fprintf(stderr, "An error occurred: %s\n", error.message);
		}
		mongoc_cursor_destroy(cursor);
		mongoc_client_pool_push(m_pool, client);																						//�Ż�һ��mongo����
	}
	else if (m_backtestmode == BAR_MODE)
	{
		databasename = "CTPMinuteDb";
		const char* collectionsname = collectionstring.c_str();

		mongoc_cursor_t *cursor;
		bson_error_t error;
		const bson_t *doc;

		bson_t parent;
		bson_t child;
		mongoc_collection_t *collection;

		bson_init(&parent);
		BSON_APPEND_DOCUMENT_BEGIN(&parent, "datetime", &child);
		BSON_APPEND_TIME_T(&child, "$gt", startDatetime);
		BSON_APPEND_TIME_T(&child, "$lt", endDatetime);
		bson_append_document_end(&parent, &child);

		char * str = bson_as_json(&parent, NULL);
		//	printf("\n%s\n", str);

		mongoc_client_t    *client = mongoc_client_pool_pop(m_pool);																				//ȡһ��mongo����

		collection = mongoc_client_get_collection(client, databasename, collectionsname);

		cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 0, 0, &parent, NULL, NULL);

		while (mongoc_cursor_next(cursor, &doc))
		{
			str = bson_as_json(doc, NULL);
			std::string s = str;
			std::string err;

			auto json = json11::Json::parse(s, err);
			if (!err.empty())
			{
				mongoc_cursor_destroy(cursor);
				mongoc_client_pool_push(m_pool, client);																						//�Ż�һ��mongo����
				return;
			}
			BarData bardata;
			bardata.symbol = json["symbol"].string_value();
			bardata.exchange = json["exchange"].string_value();
			bardata.open = json["open"].number_value();
			bardata.high = json["high"].number_value();
			bardata.low = json["low"].number_value();
			bardata.close = json["close"].number_value();
			bardata.volume = json["volume"].number_value();

			json11::Json::object datetime = json["datetime"].object_items();
			bardata.unixdatetime = datetime["$date"].number_value() / 1000;

			bardata.date = json["date"].string_value();
			bardata.time = json["time"].string_value();

			bardata.openPrice = json["openPrice"].number_value();//���տ�
			bardata.highPrice = json["highPrice"].number_value();//���ո�
			bardata.lowPrice = json["lowPrice"].number_value();//���յ�
			bardata.preClosePrice = json["preClosePrice"].number_value();//����

			bardata.upperLimit = json["upperLimit"].number_value();//��ͣ
			bardata.lowerLimit = json["lowerLimit"].number_value();//��ͣ

			bardata.openInterest = json["openInterest"].number_value();//�ֲ�

			m_Bardatavector.push_back(bardata);

			//		printf("%s\n", str);
			bson_free(str);
		}
		if (mongoc_cursor_error(cursor, &error)) {
			writeCtaLog(error.message, "backtest");
		}
		mongoc_cursor_destroy(cursor);
		mongoc_client_pool_push(m_pool, client);																						//�Ż�һ��mongo����
	}
}

void BacktestEngine::processTickEvent(std::shared_ptr<Event>e)
{
	std::shared_ptr<Event_Backtest_Tick> eTick = std::static_pointer_cast<Event_Backtest_Tick>(e);
	TickData tick = eTick->tick;
	CrossLimitOrder(tick);

	std::unique_lock<std::mutex>lck(m_tickstrategymtx);
	if (m_tickstrategymap.find(tick.symbol) != m_tickstrategymap.end())
	{
		for (std::vector<StrategyTemplate*>::iterator it = m_tickstrategymap[tick.symbol].begin(); it != m_tickstrategymap[tick.symbol].end(); it++)
		{
			(*it)->onTick(tick);
			//VarPlotRecord();//��ȡ����  tick������ʱstrategytemplate��û��Ū
		}
	}
	lck.unlock();
	std::unique_lock<std::mutex>lck2(m_workermutex);
	working_worker -= 1;
	if (working_worker == 0)
	{
		m_cv.notify_all();
	}
	//�ж��Ƿ���һ�����һ������
	const time_t datetime = (tick.unixdatetime);
	tm *ptr = localtime(&datetime);
	if (ptr->tm_hour == 14 && ptr->tm_min == 59)
	{
		RecordCapital(tick);
	}
	RecordPNL(tick);
}

void BacktestEngine::processBarEvent(std::shared_ptr<Event>e)
{
	std::shared_ptr<Event_Backtest_Bar> eBar = std::static_pointer_cast<Event_Backtest_Bar>(e);
	BarData bar = eBar->bar;
	CrossLimitOrder(bar);

	std::unique_lock<std::mutex>lck(m_tickstrategymtx);
	if (m_tickstrategymap.find(bar.symbol) != m_tickstrategymap.end())
	{
		for (std::vector<StrategyTemplate*>::iterator it = m_tickstrategymap[bar.symbol].begin(); it != m_tickstrategymap[bar.symbol].end(); it++)
		{
			(*it)->onBar(bar);
			VarPlotRecord(*it, bar);//��ȡ����
		}
	}
	std::unique_lock<std::mutex>lck2(m_workermutex);
	working_worker -= 1;
	if (working_worker == 0)
	{
		m_cv.notify_all();
	}
	//�ж��Ƿ���һ�����һ������
	const time_t datetime = (bar.unixdatetime);
	tm *ptr = localtime(&datetime);
	if (ptr->tm_hour == 14 && ptr->tm_min == 59)
	{
		RecordCapital(bar);
	}
	RecordPNL(bar);
}

void BacktestEngine::CrossLimitOrder(const TickData&data)
{
	double	buyCrossPrice = data.askprice1;
	double	sellCrossPrice = data.bidprice1;
	double	buyBestCrossPrice = data.askprice1;
	double	sellBestCrossPrice = data.bidprice1;
	std::unique_lock<std::mutex>lck(m_ordermapmtx);
	for (std::map<std::string, std::shared_ptr<Event_Order>>::iterator iter = m_WorkingOrdermap.begin(); iter != m_WorkingOrdermap.end();)
	{
		if (data.symbol == iter->second->symbol)
		{
			bool buyCross = (iter->second->direction == DIRECTION_LONG && iter->second->price >= buyCrossPrice);
			bool sellCross = (iter->second->direction == DIRECTION_SHORT && iter->second->price <= sellCrossPrice);

			//��������˳ɽ�
			if (buyCross || sellCross)
			{
				m_tradeCount += 1;
				std::string tradeID = iter->first;
				std::shared_ptr<Event_Trade>trade = std::make_shared<Event_Trade>();
				trade->symbol = iter->second->symbol;
				trade->tradeID = tradeID;
				trade->orderID = iter->second->orderID;
				trade->direction = iter->second->direction;
				trade->offset = iter->second->offset;
				trade->volume = iter->second->totalVolume;
				trade->tradeTime = time_t2str(m_datetime);
				std::unique_lock<std::mutex>lck2(m_orderStrategymtx);
				if (buyCross)
				{
					trade->price = std::min(iter->second->price, buyBestCrossPrice);
					m_orderStrategymap[iter->first]->changeposmap(iter->second->symbol, m_orderStrategymap[iter->first]->getpos(iter->second->symbol) + trade->volume);
				}
				else if (sellCross)
				{
					trade->price = std::max(iter->second->price, sellBestCrossPrice);
					m_orderStrategymap[iter->first]->changeposmap(iter->second->symbol, m_orderStrategymap[iter->first]->getpos(iter->second->symbol) - trade->volume);
				}
				m_orderStrategymap[iter->first]->onTrade(trade);

				Settlement(trade, m_orderStrategymap);

				iter->second->tradedVolume = iter->second->totalVolume;
				iter->second->status = STATUS_ALLTRADED;
				m_Ordermap[iter->first]->status = STATUS_ALLTRADED;

				m_orderStrategymap[iter->first]->onOrder(iter->second);
				m_WorkingOrdermap.erase(iter++); //#1 

			}
			else
			{
				iter++;
			}
		}
		else
		{
			iter++;
		}
	}
}

void BacktestEngine::CrossLimitOrder(const BarData&data)
{
	double	buyCrossPrice = data.low;
	double	sellCrossPrice = data.high;
	double	buyBestCrossPrice = data.open;
	double	sellBestCrossPrice = data.open;
	std::unique_lock<std::mutex>lck(m_ordermapmtx);
	for (std::map<std::string, std::shared_ptr<Event_Order>>::iterator iter = m_WorkingOrdermap.begin(); iter != m_WorkingOrdermap.end();)
	{
		if (data.symbol == iter->second->symbol)
		{
			bool buyCross = (iter->second->direction == DIRECTION_LONG && iter->second->price >= buyCrossPrice);
			bool sellCross = (iter->second->direction == DIRECTION_SHORT && iter->second->price <= sellCrossPrice);

			//��������˳ɽ�
			if (buyCross || sellCross)
			{
				m_tradeCount += 1;
				std::string tradeID = iter->first;
				std::shared_ptr<Event_Trade>trade = std::make_shared<Event_Trade>();
				trade->symbol = iter->second->symbol;
				trade->tradeID = tradeID;
				trade->orderID = iter->second->orderID;
				trade->direction = iter->second->direction;
				trade->offset = iter->second->offset;
				trade->volume = iter->second->totalVolume;
				trade->tradeTime = time_t2str(m_datetime);
				std::unique_lock<std::mutex>lck2(m_orderStrategymtx);
				if (buyCross)
				{
					trade->price = std::min(iter->second->price, buyBestCrossPrice);
					m_orderStrategymap[iter->first]->changeposmap(iter->second->symbol, m_orderStrategymap[iter->first]->getpos(iter->second->symbol) + trade->volume);
				}
				else if (sellCross)
				{
					trade->price = std::max(iter->second->price, sellBestCrossPrice);
					m_orderStrategymap[iter->first]->changeposmap(iter->second->symbol, m_orderStrategymap[iter->first]->getpos(iter->second->symbol) - trade->volume);
				}
				m_orderStrategymap[iter->first]->onTrade(trade);

				savetraderecord(m_orderStrategymap[trade->orderID]->getparam("name"), trade);

				Settlement(trade, m_orderStrategymap);

				iter->second->tradedVolume = iter->second->totalVolume;
				iter->second->status = STATUS_ALLTRADED;
				m_Ordermap[iter->first]->status = STATUS_ALLTRADED;

				m_orderStrategymap[iter->first]->onOrder(iter->second);
				m_WorkingOrdermap.erase(iter++); //#1 
			}
			else
			{
				iter++;
			}
		}
		else
		{
			iter++;
		}
	}
}

void BacktestEngine::Settlement(std::shared_ptr<Event_Trade>etrade, std::map<std::string, StrategyTemplate*>orderStrategymap)
{
	//����
	if (orderStrategymap[etrade->orderID] == nullptr)
	{
		return;
	}
	std::string strategyname = orderStrategymap[etrade->orderID]->getparam("name");
	std::vector<Event_Trade>longTrade_v;//δƽ�ֶ�ͷ����
	std::vector<Event_Trade>shortTrade_v;//δƽ�ֿ�ͷ����
	std::vector<TradingResult>resultList;//���׽���б�

	std::unique_lock<std::mutex>lck(m_tradelistmtx);
	if (m_tradelist_memory.find(strategyname) != m_tradelist_memory.end())
	{
		if (m_tradelist_memory[strategyname].find(etrade->symbol) != m_tradelist_memory[strategyname].end())
		{
			for (std::vector<Event_Trade>::iterator tradeiter = m_tradelist_memory[strategyname][etrade->symbol].begin(); tradeiter != m_tradelist_memory[strategyname][etrade->symbol].end(); tradeiter++)
			{
				if ((tradeiter)->direction == DIRECTION_LONG)
				{
					longTrade_v.push_back(*tradeiter);
				}
				else
				{
					shortTrade_v.push_back(*tradeiter);
				}
			}
			m_tradelist_memory[strategyname][etrade->symbol].clear();//	��գ�����������¼����µ�vector
		}
	}
	lck.unlock();
	if ((etrade)->direction == DIRECTION_LONG)
	{
		if (shortTrade_v.empty())
		{
			longTrade_v.push_back(*etrade);
		}
		else
		{
			Event_Trade exitTrade = *etrade;

			while (true)
			{
				Event_Trade *entryTrade = &shortTrade_v[0];

				//���㿪ƽ�ֽ���
				double closedVolume = std::min(exitTrade.volume, entryTrade->volume);
				TradingResult result = TradingResult(entryTrade->price, entryTrade->tradeTime, exitTrade.price, exitTrade.tradeTime, -closedVolume, symbol_rate[Utils::regMySymbol(entryTrade->symbol)], m_slippage[Utils::regMySymbol(entryTrade->symbol)], symbol_size[Utils::regMySymbol(etrade->symbol)]);
				resultList.push_back(result);

				//����δ���㲿��
				entryTrade->volume -= closedVolume;
				exitTrade.volume -= closedVolume;

				//������ֽ��׾�����
				if (entryTrade->volume == 0)
				{
					shortTrade_v.erase(shortTrade_v.begin());
				}

				//���ƽ�ֽ����Ѿ������˳�ѭ��
				if (exitTrade.volume == 0)
				{
					break;
				}

				//���ƽ��δȫ������
				if (exitTrade.volume)
				{
					// �ҿ��ֽ����Ѿ�ȫ�������꣬��ƽ�ֽ���ʣ��Ĳ���
					// �����µķ��򿪲ֽ��ף���ӵ�������
					if (shortTrade_v.empty())
					{
						longTrade_v.push_back(exitTrade);
						break;
					}
				}
			}
		}
	}
	//��ͷ
	else
	{
		//������޶�ͷ����
		if (longTrade_v.empty())
		{
			shortTrade_v.push_back(*etrade);
		}
		else
		{
			Event_Trade exitTrade = *etrade;
			while (true)
			{
				Event_Trade *entryTrade = &longTrade_v[0];
				//���㿪ƽ�ֽ���
				double closedVolume = std::min(exitTrade.volume, entryTrade->volume);
				TradingResult result = TradingResult(entryTrade->price, entryTrade->tradeTime, exitTrade.price, entryTrade->tradeTime, closedVolume, symbol_rate[Utils::regMySymbol(entryTrade->symbol)], m_slippage[Utils::regMySymbol(entryTrade->symbol)], symbol_size[Utils::regMySymbol(etrade->symbol)]);
				resultList.push_back(result);

				//����δ���㲿��

				entryTrade->volume -= closedVolume;
				exitTrade.volume -= closedVolume;

				//������ֽ����Ѿ�ȫ�����㣬����б����Ƴ�
				if (entryTrade->volume == 0)
				{
					longTrade_v.erase(longTrade_v.begin());
				}

				//���ƽ�ֽ����Ѿ�ȫ�����㣬���˳�ѭ��
				if (exitTrade.volume == 0)
				{
					break;
				}
				//���ƽ�ֽ���δȫ������
				if (exitTrade.volume)
				{
					//�ҿ��ֽ����Ѿ�ȫ�������꣬��ƽ�ֽ���ʣ��Ĳ���
					// �����µķ��򿪲ֽ��ף���ӵ�������
					if (longTrade_v.empty())
					{
						shortTrade_v.push_back(exitTrade);
						break;
					}
				}
			}
		}
	}
	//���һ����
	lck.lock();
	if (!shortTrade_v.empty())
	{
		//����
		std::vector<Event_Trade>trade_v;
		for (std::vector<Event_Trade>::iterator shortTradeit = shortTrade_v.begin(); shortTradeit != shortTrade_v.end(); shortTradeit++)
		{
			trade_v.push_back(*shortTradeit);
		}
		if (m_tradelist_memory.find(strategyname) != m_tradelist_memory.end())
		{
			m_tradelist_memory[strategyname][etrade->symbol] = trade_v;
		}
		else
		{
			std::map<std::string, std::vector<Event_Trade>>symbol_vector;
			symbol_vector.insert(std::pair<std::string, std::vector<Event_Trade>>(etrade->symbol, trade_v));
			m_tradelist_memory.insert(std::pair<std::string, std::map<std::string, std::vector<Event_Trade>>>(strategyname, symbol_vector));
		}

	}
	if (!longTrade_v.empty())
	{
		std::vector<Event_Trade>trade_v;
		for (std::vector<Event_Trade>::iterator longTradeit = longTrade_v.begin(); longTradeit != longTrade_v.end(); longTradeit++)
		{
			trade_v.push_back(*longTradeit);
		}
		if (m_tradelist_memory.find(strategyname) != m_tradelist_memory.end())
		{
			m_tradelist_memory[strategyname][etrade->symbol] = trade_v;
		}
		else
		{
			std::map<std::string, std::vector<Event_Trade>>symbol_vector;
			symbol_vector.insert(std::pair<std::string, std::vector<Event_Trade>>(etrade->symbol, trade_v));
			m_tradelist_memory.insert(std::pair<std::string, std::map<std::string, std::vector<Event_Trade>>>(strategyname, symbol_vector));
		}
	}

	//����ÿһ�ʽ��

	double  totalwinning = 0;
	double	maxCapital = 0;
	double	drawdown = 0;
	double	Winning = 0;
	double	Losing = 0;
	int totalResult = 0;

	double holdingwinning = 0;
	double holdingposition = 0;
	double holdingprice = 0;

	if (m_result.find(strategyname) != m_result.end())
	{

		totalwinning = m_result[strategyname][etrade->symbol].totalwinning;
		maxCapital = m_result[strategyname][etrade->symbol].maxCapital;
		drawdown = m_result[strategyname][etrade->symbol].drawdown;
		Winning = m_result[strategyname][etrade->symbol].Winning;
		Losing = m_result[strategyname][etrade->symbol].Losing;
		totalResult = m_result[strategyname][etrade->symbol].totalResult;
		//����ֲ�
		double totalcost = 0;
		for (std::vector<Event_Trade>::iterator symbol_tradememory_iter = m_tradelist_memory[strategyname][etrade->symbol].begin();
			symbol_tradememory_iter != m_tradelist_memory[strategyname][etrade->symbol].end(); symbol_tradememory_iter++)
		{
			totalcost += ((symbol_tradememory_iter)->volume) * ((symbol_tradememory_iter)->price);
			if ((symbol_tradememory_iter)->direction == DIRECTION_LONG)
			{
				holdingposition += (symbol_tradememory_iter)->volume;
			}
			else if ((symbol_tradememory_iter)->direction == DIRECTION_SHORT)
			{
				holdingposition -= (symbol_tradememory_iter)->volume;
			}

		}
		holdingprice = totalcost / std::abs(holdingposition);

		for (std::vector<TradingResult>::iterator resultiter = resultList.begin(); resultiter != resultList.end(); resultiter++)
		{
			totalwinning += resultiter->m_pnl;
			maxCapital = std::max(totalwinning, maxCapital);
			drawdown = totalwinning - maxCapital;
			totalResult += 1;

			if (resultiter->m_pnl >= 0)
			{
				Winning += resultiter->m_pnl;
			}
			else
			{
				Losing += resultiter->m_pnl;
			}
		}
		//�洢���
		m_result[strategyname][etrade->symbol].totalwinning = totalwinning;
		m_result[strategyname][etrade->symbol].maxCapital = maxCapital;
		m_result[strategyname][etrade->symbol].drawdown = drawdown;
		m_result[strategyname][etrade->symbol].Winning = Winning;
		m_result[strategyname][etrade->symbol].Losing = Losing;
		m_result[strategyname][etrade->symbol].totalResult = totalResult;
		m_result[strategyname][etrade->symbol].holdingwinning = holdingwinning;//�ֲ�ӯ��
		m_result[strategyname][etrade->symbol].holdingposition = holdingposition;
		m_result[strategyname][etrade->symbol].holdingprice = holdingprice;
	}
	else
	{
		Result res;
		UnitResult unitres;
		unitres.totalwinning = totalwinning;
		unitres.maxCapital = maxCapital;
		unitres.drawdown = drawdown;
		unitres.Winning = Winning;
		unitres.Losing = Losing;
		unitres.holdingwinning = holdingwinning;//�ֲ�ӯ��
		unitres.holdingposition = holdingposition;
		unitres.holdingprice = holdingprice;
		res.insert(std::pair<std::string, UnitResult>(etrade->symbol, unitres));
		m_result.insert(std::pair<std::string, Result>(strategyname, res));

		//����ֲ�
		double totalcost = 0;
		for (std::vector<Event_Trade>::iterator symbol_tradememory_iter = m_tradelist_memory[strategyname][etrade->symbol].begin();
			symbol_tradememory_iter != m_tradelist_memory[strategyname][etrade->symbol].end(); symbol_tradememory_iter++)
		{
			totalcost += ((symbol_tradememory_iter)->volume) * ((symbol_tradememory_iter)->price);
			if ((symbol_tradememory_iter)->direction == DIRECTION_LONG)
			{
				holdingposition += (symbol_tradememory_iter)->volume;
			}
			else
			{
				holdingposition -= (symbol_tradememory_iter)->volume;
			}
		}
		holdingprice = totalcost / std::abs(holdingposition);

		for (std::vector<TradingResult>::iterator resultiter = resultList.begin(); resultiter != resultList.end(); resultiter++)
		{
			totalwinning += resultiter->m_pnl;
			maxCapital = std::max(totalwinning, maxCapital);
			drawdown = totalwinning - maxCapital;
			totalResult += 1;

			if (resultiter->m_pnl >= 0)
			{
				Winning += resultiter->m_pnl;
			}
			else
			{
				Losing += resultiter->m_pnl;
			}
		}
		//�洢���
		m_result[strategyname][etrade->symbol].totalwinning = totalwinning;
		m_result[strategyname][etrade->symbol].maxCapital = maxCapital;
		m_result[strategyname][etrade->symbol].drawdown = drawdown;
		m_result[strategyname][etrade->symbol].Winning = Winning;
		m_result[strategyname][etrade->symbol].Losing = Losing;
		m_result[strategyname][etrade->symbol].totalResult = totalResult;
		m_result[strategyname][etrade->symbol].holdingwinning = holdingwinning;//�ֲ�ӯ��
		m_result[strategyname][etrade->symbol].holdingposition = holdingposition;
		m_result[strategyname][etrade->symbol].holdingprice = holdingprice;
	}
	lck.unlock();
}

void BacktestEngine::RecordCapital(const TickData &data)
{
	std::unique_lock<std::mutex>lck(m_plotmtx);
	std::unique_lock<std::mutex>lck2(m_resultmtx);
	for (std::map<std::string, Result>::iterator it = m_result.begin(); it != m_result.end(); it++)
	{
		std::string strategyname = it->first;
		std::string symbol = data.symbol;
		for (std::map<std::string, UnitResult>::iterator iter = it->second.begin(); iter != it->second.end(); iter++)
		{
			//�������к�Լ
			if (iter->first == data.symbol)
			{
				if (iter->second.holdingposition > 0)
				{
					iter->second.holdingwinning = (data.lastprice - iter->second.holdingprice)*iter->second.holdingposition*symbol_size[Utils::regMySymbol(data.symbol)];
				}
				else if (iter->second.holdingposition < 0)
				{
					iter->second.holdingwinning = (iter->second.holdingprice - data.lastprice)*(-iter->second.holdingposition)*symbol_size[Utils::regMySymbol(data.symbol)];
				}
				else if (iter->second.holdingposition == 0)
				{
					iter->second.holdingwinning = 0;
				}
				double drawdown = iter->second.drawdown;
				double holdingposition = iter->second.holdingposition;
				double holdingprice = iter->second.holdingprice;
				double holdingwinning = iter->second.holdingwinning;
				double Losing = iter->second.Losing;
				double maxCapital = iter->second.maxCapital;
				double totalResult = iter->second.totalResult;
				double totalwinning = iter->second.totalwinning;
				double Winning = iter->second.Winning;
				double holding_and_totalwinning = iter->second.totalwinning + iter->second.holdingwinning;
				double portfolio_winning = 0;
				for (std::map<std::string, Result>::iterator it_temp1 = m_result.begin(); it_temp1 != m_result.end(); it_temp1++)
				{
					for (std::map<std::string, UnitResult>::iterator it_temp2 = it_temp1->second.begin(); it_temp2 != it_temp1->second.end(); it_temp2++)
					{
						portfolio_winning += (it_temp2->second.holdingwinning + it_temp2->second.totalwinning);
					}
				}
				//////////////////////////////////////////////////////////
				bool condition1 = m_plot_data.m_holding_and_totalwinning.find(strategyname) == m_plot_data.m_holding_and_totalwinning.end();
				bool condition2 = m_plot_data.m_totalwinning.find(strategyname) == m_plot_data.m_totalwinning.end();
				bool condition3 = m_plot_data.m_Winning.find(strategyname) == m_plot_data.m_Winning.end();
				bool condition4 = m_plot_data.m_Losing.find(strategyname) == m_plot_data.m_Losing.end();
				bool condition5 = m_plot_data.m_drawdown.find(strategyname) == m_plot_data.m_drawdown.end();
				if (condition1&&condition2&&condition3&&condition4&&condition5)
				{
					//����һ��vector
					std::vector<double>v1, v2, v3, v4, v5;
					std::vector<int>v6;
					m_plot_data.m_holding_and_totalwinning.insert(std::pair<std::string, std::vector<double>>(strategyname, v1));
					m_plot_data.m_totalwinning.insert(std::pair<std::string, std::vector<double>>(strategyname, v2));
					m_plot_data.m_Winning.insert(std::pair<std::string, std::vector<double>>(strategyname, v3));
					m_plot_data.m_Losing.insert(std::pair<std::string, std::vector<double>>(strategyname, v4));
					m_plot_data.m_drawdown.insert(std::pair<std::string, std::vector<double>>(strategyname, v5));
					m_plot_data.m_capital_datetime.insert(std::pair<std::string, std::vector<int>>(strategyname, v6));
					m_plot_data.m_holding_and_totalwinning[strategyname].push_back(holding_and_totalwinning);							//��̬�Ӿ�̬ӯ��
					m_plot_data.m_totalwinning[strategyname].push_back(totalwinning);												   //��̬��ӯ��
					m_plot_data.m_Winning[strategyname].push_back(Winning);														   //ƽ����ӯ��
					m_plot_data.m_Losing[strategyname].push_back(Losing);															   //ƽ���ܿ���
					m_plot_data.m_drawdown[strategyname].push_back(drawdown);														   //�س�			
					m_plot_data.m_capital_datetime[strategyname].push_back((data.unixdatetime));
				}
				else
				{
					m_plot_data.m_holding_and_totalwinning[strategyname].push_back(holding_and_totalwinning);							//��̬�Ӿ�̬ӯ��
					m_plot_data.m_totalwinning[strategyname].push_back(totalwinning);												   //��̬��ӯ��
					m_plot_data.m_Winning[strategyname].push_back(Winning);														   //ƽ����ӯ��
					m_plot_data.m_Losing[strategyname].push_back(Losing);															   //ƽ���ܿ���
					m_plot_data.m_drawdown[strategyname].push_back(drawdown);														   //�س�		
					m_plot_data.m_capital_datetime[strategyname].push_back((data.unixdatetime));
				}
			}
		}
	}
}

void BacktestEngine::RecordCapital(const BarData &data)
{
	//����m_resultӦ�����紴����Ҫ��Ȼ�ᷢ����һ���޷���¼δ���׺�Լ�������
	std::unique_lock<std::mutex>lck(m_plotmtx);
	std::unique_lock<std::mutex>lck2(m_resultmtx);

	for (std::map<std::string, Result>::iterator it = m_result.begin(); it != m_result.end(); it++)
	{
		std::string strategyname = it->first;
		std::string symbol = data.symbol;
		for (std::map<std::string, UnitResult>::iterator iter = it->second.begin(); iter != it->second.end(); iter++)
		{
			//�������к�Լ
			if (iter->first == data.symbol)
			{
				if (iter->second.holdingposition > 0)
				{
					iter->second.holdingwinning = (data.close - iter->second.holdingprice)*iter->second.holdingposition*symbol_size[Utils::regMySymbol(data.symbol)];
				}
				else if (iter->second.holdingposition < 0)
				{
					iter->second.holdingwinning = (iter->second.holdingprice - data.close)*(-iter->second.holdingposition)*symbol_size[Utils::regMySymbol(data.symbol)];
				}
				else if (iter->second.holdingposition == 0)
				{
					iter->second.holdingwinning = 0;
				}
				double drawdown = iter->second.drawdown;
				double holdingposition = iter->second.holdingposition;
				double holdingprice = iter->second.holdingprice;
				double holdingwinning = iter->second.holdingwinning;
				double Losing = iter->second.Losing;
				double maxCapital = iter->second.maxCapital;
				double totalResult = iter->second.totalResult;
				double totalwinning = iter->second.totalwinning;
				double Winning = iter->second.Winning;
				double holding_and_totalwinning = iter->second.totalwinning + iter->second.holdingwinning;
				double portfolio_winning = 0;
				for (std::map<std::string, Result>::iterator it_temp1 = m_result.begin(); it_temp1 != m_result.end(); it_temp1++)
				{
					for (std::map<std::string, UnitResult>::iterator it_temp2 = it_temp1->second.begin(); it_temp2 != it_temp1->second.end(); it_temp2++)
					{
						portfolio_winning += (it_temp2->second.holdingwinning + it_temp2->second.totalwinning);
					}
				}
				//////////////////////////////////////////////////////////
				bool condition1 = m_plot_data.m_holding_and_totalwinning.find(strategyname) == m_plot_data.m_holding_and_totalwinning.end();
				bool condition2 = m_plot_data.m_totalwinning.find(strategyname) == m_plot_data.m_totalwinning.end();
				bool condition3 = m_plot_data.m_Winning.find(strategyname) == m_plot_data.m_Winning.end();
				bool condition4 = m_plot_data.m_Losing.find(strategyname) == m_plot_data.m_Losing.end();
				bool condition5 = m_plot_data.m_drawdown.find(strategyname) == m_plot_data.m_drawdown.end();
				if (condition1&&condition2&&condition3&&condition4&&condition5)
				{
					//����һ��vector
					std::vector<double>v1, v2, v3, v4, v5;
					std::vector<int>v6;
					m_plot_data.m_holding_and_totalwinning.insert(std::pair<std::string, std::vector<double>>(strategyname, v1));
					m_plot_data.m_totalwinning.insert(std::pair<std::string, std::vector<double>>(strategyname, v2));
					m_plot_data.m_Winning.insert(std::pair<std::string, std::vector<double>>(strategyname, v3));
					m_plot_data.m_Losing.insert(std::pair<std::string, std::vector<double>>(strategyname, v4));
					m_plot_data.m_drawdown.insert(std::pair<std::string, std::vector<double>>(strategyname, v5));
					m_plot_data.m_capital_datetime.insert(std::pair<std::string, std::vector<int>>(strategyname, v6));
					m_plot_data.m_holding_and_totalwinning[strategyname].push_back(holding_and_totalwinning);							//��̬�Ӿ�̬ӯ��
					m_plot_data.m_totalwinning[strategyname].push_back(totalwinning);												   //��̬��ӯ��
					m_plot_data.m_Winning[strategyname].push_back(Winning);														   //ƽ����ӯ��
					m_plot_data.m_Losing[strategyname].push_back(Losing);															   //ƽ���ܿ���
					m_plot_data.m_drawdown[strategyname].push_back(drawdown);														   //�س�			
					m_plot_data.m_capital_datetime[strategyname].push_back((data.unixdatetime));
				}
				else
				{
					m_plot_data.m_holding_and_totalwinning[strategyname].push_back(holding_and_totalwinning);							//��̬�Ӿ�̬ӯ��
					m_plot_data.m_totalwinning[strategyname].push_back(totalwinning);												   //��̬��ӯ��
					m_plot_data.m_Winning[strategyname].push_back(Winning);														   //ƽ����ӯ��
					m_plot_data.m_Losing[strategyname].push_back(Losing);															   //ƽ���ܿ���
					m_plot_data.m_drawdown[strategyname].push_back(drawdown);														   //�س�		
					m_plot_data.m_capital_datetime[strategyname].push_back((data.unixdatetime));
				}
			}
		}
	}
}

void BacktestEngine::RecordPNL(const TickData &data)
{
	//����m_resultӦ�����紴����Ҫ��Ȼ�ᷢ����һ���޷���¼δ���׺�Լ�������
	std::unique_lock<std::mutex>lck(m_varplotrecordmtx);
	std::unique_lock<std::mutex>lck2(m_resultmtx);

	for (std::map<std::string, Result>::iterator it = m_result.begin(); it != m_result.end(); it++)
	{
		std::string strategyname = it->first;
		std::string symbol = data.symbol;
		for (std::map<std::string, UnitResult>::iterator iter = it->second.begin(); iter != it->second.end(); iter++)
		{
			//�������к�Լ
			if (iter->first == data.symbol)
			{
				if (iter->second.holdingposition > 0)
				{
					iter->second.holdingwinning = (data.lastprice - iter->second.holdingprice)*iter->second.holdingposition*symbol_size[Utils::regMySymbol(data.symbol)];
				}
				else if (iter->second.holdingposition < 0)
				{
					iter->second.holdingwinning = (iter->second.holdingprice - data.lastprice)*(-iter->second.holdingposition)*symbol_size[Utils::regMySymbol(data.symbol)];
				}
				else if (iter->second.holdingposition == 0)
				{
					iter->second.holdingwinning = 0;
				}
				double drawdown = iter->second.drawdown;
				double holdingposition = iter->second.holdingposition;
				double holdingprice = iter->second.holdingprice;
				double holdingwinning = iter->second.holdingwinning;
				double Losing = iter->second.Losing;
				double maxCapital = iter->second.maxCapital;
				double totalResult = iter->second.totalResult;
				double totalwinning = iter->second.totalwinning;
				double Winning = iter->second.Winning;
				double holding_and_totalwinning = iter->second.totalwinning + iter->second.holdingwinning;
				double portfolio_winning = 0;
				for (std::map<std::string, Result>::iterator it_temp1 = m_result.begin(); it_temp1 != m_result.end(); it_temp1++)
				{
					for (std::map<std::string, UnitResult>::iterator it_temp2 = it_temp1->second.begin(); it_temp2 != it_temp1->second.end(); it_temp2++)
					{
						portfolio_winning += (it_temp2->second.holdingwinning + it_temp2->second.totalwinning);
					}
				}
				//////////////////////////////////////////////////////////

				if (m_Vardata.m_strategy_varplotrecord_pnl.find(strategyname) == m_Vardata.m_strategy_varplotrecord_pnl.end())
				{
					//����һ��vector
					std::vector<double>v;
					m_Vardata.m_strategy_varplotrecord_pnl.insert(std::pair<std::string, std::vector<double>>(strategyname, v));


					m_Vardata.m_strategy_varplotrecord_pnl[strategyname].push_back((holding_and_totalwinning));
				}
				else
				{
					m_Vardata.m_strategy_varplotrecord_pnl[strategyname].push_back((holding_and_totalwinning));
				}
			}
		}
	}
}

void BacktestEngine::RecordPNL(const BarData &data)
{
	//����m_resultӦ�����紴����Ҫ��Ȼ�ᷢ����һ���޷���¼δ���׺�Լ�������
	std::unique_lock<std::mutex>lck(m_varplotrecordmtx);
	std::unique_lock<std::mutex>lck2(m_resultmtx);

	for (std::map<std::string, Result>::iterator it = m_result.begin(); it != m_result.end(); it++)
	{
		std::string strategyname = it->first;
		std::string symbol = data.symbol;
		for (std::map<std::string, UnitResult>::iterator iter = it->second.begin(); iter != it->second.end(); iter++)
		{
			//�������к�Լ
			if (iter->first == data.symbol)
			{
				if (iter->second.holdingposition > 0)
				{
					iter->second.holdingwinning = (data.close - iter->second.holdingprice)*iter->second.holdingposition*symbol_size[Utils::regMySymbol(data.symbol)];
				}
				else if (iter->second.holdingposition < 0)
				{
					iter->second.holdingwinning = (iter->second.holdingprice - data.close)*(-iter->second.holdingposition)*symbol_size[Utils::regMySymbol(data.symbol)];
				}
				else if (iter->second.holdingposition == 0)
				{
					iter->second.holdingwinning = 0;
				}
				double drawdown = iter->second.drawdown;
				double holdingposition = iter->second.holdingposition;
				double holdingprice = iter->second.holdingprice;
				double holdingwinning = iter->second.holdingwinning;
				double Losing = iter->second.Losing;
				double maxCapital = iter->second.maxCapital;
				double totalResult = iter->second.totalResult;
				double totalwinning = iter->second.totalwinning;
				double Winning = iter->second.Winning;
				double holding_and_totalwinning = iter->second.totalwinning + iter->second.holdingwinning;
				double portfolio_winning = 0;
				for (std::map<std::string, Result>::iterator it_temp1 = m_result.begin(); it_temp1 != m_result.end(); it_temp1++)
				{
					for (std::map<std::string, UnitResult>::iterator it_temp2 = it_temp1->second.begin(); it_temp2 != it_temp1->second.end(); it_temp2++)
					{
						portfolio_winning += (it_temp2->second.holdingwinning + it_temp2->second.totalwinning);
					}
				}
				//////////////////////////////////////////////////////////

				if (m_Vardata.m_strategy_varplotrecord_pnl.find(strategyname) == m_Vardata.m_strategy_varplotrecord_pnl.end())
				{
					//����һ��vector
					std::vector<double>v;
					m_Vardata.m_strategy_varplotrecord_pnl.insert(std::pair<std::string, std::vector<double>>(strategyname, v));


					m_Vardata.m_strategy_varplotrecord_pnl[strategyname].push_back((holding_and_totalwinning));
				}
				else
				{
					m_Vardata.m_strategy_varplotrecord_pnl[strategyname].push_back((holding_and_totalwinning));
				}
			}
		}
	}
}

TradingResult::TradingResult(double entryPrice, std::string entryDt, double exitPrice, std::string exitDt, double volume, double rate, double slippage, double size)
{
	//����
	m_entryPrice = entryPrice;  //���ּ۸�
	m_exitPrice = exitPrice;    // ƽ�ּ۸�
	m_entryDt = entryDt;        // ����ʱ��datetime
	m_exitDt = exitDt;          // ƽ��ʱ��
	m_volume = volume;			//���������� + / -������
	m_turnover = (entryPrice + exitPrice)*size*abs(volume);   //�ɽ����
	m_commission = m_turnover*rate;                                // �����ѳɱ�
	m_slippage = slippage * 2 * size*abs(volume);                        // ����ɱ�
	m_pnl = ((m_exitPrice - m_entryPrice) * volume * size-m_commission-m_slippage);  //��ӯ��
};

//�ṩ��Strategytemplate
std::vector<std::string> BacktestEngine::sendOrder(std::string symbol, std::string orderType, double price, double volume, StrategyTemplate* Strategy)
{
	std::unique_lock<std::mutex>lck(m_ordermapmtx);
	std::unique_lock<std::mutex>lck2(m_orderStrategymtx);
	std::shared_ptr<Event_Order> req = std::make_shared<Event_Order>();
	req->symbol = symbol;
	req->price = price;
	req->totalVolume = volume;
	req->status = STATUS_WAITING;
	if (orderType == CTAORDER_BUY)
	{
		req->direction = DIRECTION_LONG;//����
		req->offset = OFFSET_OPEN;//����
		//
		//����
		std::string orderID = Utils::doubletostring(m_limitorderCount++); //
		req->orderID = orderID;
		m_WorkingOrdermap.insert(std::pair<std::string, std::shared_ptr<Event_Order>>(orderID, req));
		m_Ordermap.insert(std::pair<std::string, std::shared_ptr<Event_Order>>(orderID, req));

		m_orderStrategymap.insert(std::pair<std::string, StrategyTemplate*>(orderID, Strategy));
		std::vector<std::string>result;
		result.push_back(orderID);
		return result;
	}
	else if (orderType == CTAORDER_SELL)
	{
		req->direction = DIRECTION_SHORT;//ƽ��
		req->offset = OFFSET_CLOSE;
		std::string orderID = Utils::doubletostring(m_limitorderCount++); //
		req->orderID = orderID;
		m_WorkingOrdermap.insert(std::pair<std::string, std::shared_ptr<Event_Order>>(orderID, req));
		m_Ordermap.insert(std::pair<std::string, std::shared_ptr<Event_Order>>(orderID, req));

		m_orderStrategymap.insert(std::pair<std::string, StrategyTemplate*>(orderID, Strategy));
		std::vector<std::string>result;
		result.push_back(orderID);
		return result;
	}
	else if (orderType == CTAORDER_SHORT)
	{
		//����
		req->direction = DIRECTION_SHORT;
		req->offset = OFFSET_OPEN;
		//����

		std::string orderID = Utils::doubletostring(m_limitorderCount++); //
		req->orderID = orderID;
		m_WorkingOrdermap.insert(std::pair<std::string, std::shared_ptr<Event_Order>>(orderID, req));
		m_Ordermap.insert(std::pair<std::string, std::shared_ptr<Event_Order>>(orderID, req));

		m_orderStrategymap.insert(std::pair<std::string, StrategyTemplate*>(orderID, Strategy));
		std::vector<std::string>result;
		result.push_back(orderID);
		return result;
	}
	else if (orderType == CTAORDER_COVER)
	{
		//ƽ��
		req->direction = DIRECTION_LONG;
		req->offset = OFFSET_CLOSE;
		std::string orderID = Utils::doubletostring(m_limitorderCount++); //
		req->orderID = orderID;
		m_WorkingOrdermap.insert(std::pair<std::string, std::shared_ptr<Event_Order>>(orderID, req));
		m_Ordermap.insert(std::pair<std::string, std::shared_ptr<Event_Order>>(orderID, req));
		m_orderStrategymap.insert(std::pair<std::string, StrategyTemplate*>(orderID, Strategy));
		std::vector<std::string>result;
		result.push_back(orderID);
		return result;
	}
}

void BacktestEngine::cancelOrder(std::string orderID, std::string gatewayname)
{
	std::unique_lock<std::mutex>lck(m_ordermapmtx);
	if (m_WorkingOrdermap.find(orderID) != m_WorkingOrdermap.end())
	{
		std::shared_ptr<Event_Order>order = m_WorkingOrdermap[orderID];
		if (order != nullptr)
		{
			//������Ч
			if (!(order->status == STATUS_ALLTRADED || order->status == STATUS_CANCELLED))
			{
				//�ɳ���״̬
				m_Ordermap[orderID]->status = STATUS_CANCELLED;
				m_WorkingOrdermap.erase(orderID);
			}
		}
	}
}

void BacktestEngine::writeCtaLog(std::string msg, std::string gatewayname)
{
	emit WriteLog(str2qstr(msg + " " + gatewayname));
}

void BacktestEngine::PutEvent(std::shared_ptr<Event>e)
{
	m_eventengine->Put(e);
}

std::vector<TickData> BacktestEngine::loadTick(std::string tickDbName, std::string symbol, int days)
{
	std::vector<TickData>datavector;
	if (symbol == " " || symbol == "")
	{
		return datavector;
	}
	const char* databasename = tickDbName.c_str();
	const char* collectionsname = symbol.c_str();
	auto targetday = startDatetime - (days * 24 * 3600);//��ȡ��ǰ��ϵͳʱ��

	mongoc_cursor_t *cursor;
	bson_error_t error;
	const bson_t *doc;

	// �ӿͻ��˳��л�ȡһ���ͻ���
	mongoc_client_t      *client = mongoc_client_pool_pop(m_pool);																//ȡһ��mongo����

	bson_t parent;
	bson_t child;
	mongoc_collection_t *collection;
	bson_init(&parent);
	BSON_APPEND_DOCUMENT_BEGIN(&parent, "datetime", &child);
	BSON_APPEND_TIME_T(&child, "$gt", targetday);
	BSON_APPEND_TIME_T(&child, "$lt", startDatetime);
	bson_append_document_end(&parent, &child);

	char * str = bson_as_json(&parent, NULL);
	//	printf("\n%s\n", str);

	collection = mongoc_client_get_collection(client, tickDbName.c_str(), symbol.c_str());

	cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 0, 0, &parent, NULL, NULL);

	while (mongoc_cursor_next(cursor, &doc)) {
		str = bson_as_json(doc, NULL);
		std::string s = str;
		std::string err;


		auto json = json11::Json::parse(s, err);
		if (!err.empty())
		{
			mongoc_cursor_destroy(cursor);
			mongoc_client_pool_push(m_pool, client);																						//�Ż�һ��mongo����
			return datavector;
		}
		TickData tickdata;

		tickdata.symbol = json["symbol"].string_value();
		tickdata.exchange = json["exchange"].string_value();
		tickdata.gatewayname = json["gatewayname"].string_value();

		tickdata.lastprice = json["lastprice"].number_value();
		tickdata.volume = json["volume"].number_value();
		tickdata.openInterest = json["openInterest"].number_value();

		json11::Json::object datetime = json["datetime"].object_items();
		tickdata.unixdatetime = datetime["$date"].number_value() / 1000;

		tickdata.date = json["date"].string_value();
		tickdata.time = json["time"].string_value();

		tickdata.openPrice = json["openPrice"].number_value();//���տ�
		tickdata.highPrice = json["highPrice"].number_value();//���ո�
		tickdata.lowPrice = json["lowPrice"].number_value();//���յ�
		tickdata.preClosePrice = json["preClosePrice"].number_value();//����

		tickdata.upperLimit = json["upperLimit"].number_value();//��ͣ
		tickdata.lowerLimit = json["lowerLimit"].number_value();//��ͣ

		tickdata.bidprice1 = json["bidprice1"].number_value();
		tickdata.bidprice2 = json["bidprice2"].number_value();
		tickdata.bidprice3 = json["bidprice3"].number_value();
		tickdata.bidprice4 = json["bidprice4"].number_value();
		tickdata.bidprice5 = json["bidprice5"].number_value();

		tickdata.askprice1 = json["askprice1"].number_value();
		tickdata.askprice2 = json["askprice2"].number_value();
		tickdata.askprice3 = json["askprice3"].number_value();
		tickdata.askprice4 = json["askprice4"].number_value();
		tickdata.askprice5 = json["askprice5"].number_value();

		tickdata.bidvolume1 = json["bidvolume1"].number_value();
		tickdata.bidvolume2 = json["bidvolume2"].number_value();
		tickdata.bidvolume3 = json["bidvolume3"].number_value();
		tickdata.bidvolume4 = json["bidvolume4"].number_value();
		tickdata.bidvolume5 = json["bidvolume5"].number_value();

		tickdata.askvolume1 = json["askvolume1"].number_value();
		tickdata.askvolume2 = json["askvolume2"].number_value();
		tickdata.askvolume3 = json["askvolume3"].number_value();
		tickdata.askvolume4 = json["askvolume4"].number_value();
		tickdata.askvolume5 = json["askvolume5"].number_value();

		datavector.push_back(tickdata);

		//		printf("%s\n", str);
		bson_free(str);
	}

	if (mongoc_cursor_error(cursor, &error)) {
		fprintf(stderr, "An error occurred: %s\n", error.message);
	}
	mongoc_cursor_destroy(cursor);
	mongoc_client_pool_push(m_pool, client);																						//�Ż�һ��mongo����
	return datavector;
}

std::vector<BarData> BacktestEngine::loadBar(std::string BarDbName, std::string symbol, int days)
{
	std::vector<BarData>datavector;
	if (symbol == " " || symbol == "")
	{
		return datavector;
	}
	const char* databasename = BarDbName.c_str();
	const char* collectionsname = symbol.c_str();
	auto targetday = startDatetime - (days * 24 * 3600);//��ȡ��ǰ��ϵͳʱ��

	mongoc_cursor_t *cursor;
	bson_error_t error;
	const bson_t *doc;

	bson_t parent;
	bson_t child;
	mongoc_collection_t *collection;
	bson_init(&parent);
	BSON_APPEND_DOCUMENT_BEGIN(&parent, "datetime", &child);
	BSON_APPEND_TIME_T(&child, "$gt", targetday);
	BSON_APPEND_TIME_T(&child, "$lt", startDatetime);
	bson_append_document_end(&parent, &child);

	char * str = bson_as_json(&parent, NULL);
	//	printf("\n%s\n", str);

	// �ӿͻ��˳��л�ȡһ���ͻ���
	mongoc_client_t    *client = mongoc_client_pool_pop(m_pool);																				//ȡһ��mongo����

	collection = mongoc_client_get_collection(client, BarDbName.c_str(), symbol.c_str());

	cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 0, 0, &parent, NULL, NULL);

	while (mongoc_cursor_next(cursor, &doc))
	{
		str = bson_as_json(doc, NULL);
		std::string s = str;
		std::string err;


		auto json = json11::Json::parse(s, err);
		if (!err.empty())
		{
			mongoc_cursor_destroy(cursor);
			mongoc_client_pool_push(m_pool, client);																						//�Ż�һ��mongo����
			return datavector;
		}
		BarData bardata;
		bardata.symbol = json["symbol"].string_value();
		bardata.exchange = json["exchange"].string_value();
		bardata.open = json["open"].number_value();
		bardata.high = json["high"].number_value();
		bardata.low = json["low"].number_value();
		bardata.close = json["close"].number_value();
		bardata.volume = json["volume"].number_value();

		json11::Json::object datetime = json["datetime"].object_items();
		bardata.unixdatetime = datetime["$date"].number_value() / 1000;

		bardata.date = json["date"].string_value();
		bardata.time = json["time"].string_value();

		bardata.openPrice = json["openPrice"].number_value();//���տ�
		bardata.highPrice = json["highPrice"].number_value();//���ո�
		bardata.lowPrice = json["lowPrice"].number_value();//���յ�
		bardata.preClosePrice = json["preClosePrice"].number_value();//����

		bardata.upperLimit = json["upperLimit"].number_value();//��ͣ
		bardata.lowerLimit = json["lowerLimit"].number_value();//��ͣ

		bardata.openInterest = json["openInterest"].number_value();//�ֲ�

		datavector.push_back(bardata);

		//		printf("%s\n", str);
		bson_free(str);
	}
	if (mongoc_cursor_error(cursor, &error)) {
		fprintf(stderr, "An error occurred: %s\n", error.message);
	}
	mongoc_cursor_destroy(cursor);
	mongoc_client_pool_push(m_pool, client);	//�Ż�һ��mongo����
	return datavector;
}

//�ṩ������Ľӿ�
void BacktestEngine::setStartDate(time_t datetime)
{
	startDatetime = datetime;
}

void BacktestEngine::setStopDate(time_t datetime)
{
	endDatetime = datetime;
}

void BacktestEngine::setMode(std::string mode)
{
	m_backtestmode = mode;
}

void BacktestEngine::GetTableViewData(QStandardItemModel &m_Model)
{
	std::unique_lock<std::mutex>lck(m_tickstrategymtx);
	std::unique_lock<std::mutex>lck2(m_strategymtx);
	std::unique_lock<std::mutex>lck3(m_backtestsymbolmtx);

	m_backtestsymbols.clear();
	m_tickstrategymap.clear();			//ÿ���Զ���ջ���

	writeCtaLog(">>>��ȡ�������Ҫ�ز�ĺ�Լ�����", "backtestengine");
	for (int i = 0; i < m_Model.rowCount(); i++)
	{
		std::vector<std::string> symbol_v = Utils::split(m_Model.item(i, 0)->text().toStdString(), ",");
		for (std::vector<std::string>::iterator iter = symbol_v.begin(); iter != symbol_v.end(); iter++)
		{
			if ((*iter) != "" && (*iter) != " ")
			{
				std::string strategyname = m_Model.item(i, 1)->text().toStdString();

				m_backtestsymbols.insert(*iter);
				if (m_tickstrategymap.find(*iter) == m_tickstrategymap.end())
				{
					//û��
					std::vector<StrategyTemplate*>v;
					v.push_back(m_strategymap[strategyname]);
					m_tickstrategymap.insert(std::pair<std::string, std::vector<StrategyTemplate*>>(*iter, v));
				}
				else
				{
					if (std::find(m_tickstrategymap[*iter].begin(), m_tickstrategymap[*iter].end(), m_strategymap[strategyname]) == m_tickstrategymap[*iter].end())
					{
						m_tickstrategymap[*iter].push_back(m_strategymap[strategyname]);
					}
				}
			}
		}
	}
}

std::string BacktestEngine::getparam(std::string strategyname, std::string param)
{
	std::string value;
	std::unique_lock<std::mutex>lck(m_strategymtx);
	value = m_strategymap[strategyname]->getparam(param);
	lck.unlock();
	return value;
}

void BacktestEngine::savetraderecord(std::string strategyname, std::shared_ptr<Event_Trade>etrade)
{
	//���׼�¼
	if (_access("./traderecord", 0) != -1)
	{
		std::fstream f;
		f.open("./traderecord/" + strategyname + ".csv", std::ios::app | std::ios::out);
		if (!f.is_open())
		{
			//����򲻿��ļ�
			std::shared_ptr<Event_Log>e = std::make_shared<Event_Log>();
			e->msg = "�޷����潻�׼�¼";
			e->gatewayname = "CTP";
			m_eventengine->Put(e);
			return;
		}
		std::string symbol = etrade->symbol;
		std::string direction = etrade->direction;
		std::string offset = etrade->offset;
		std::string tradetime = etrade->tradeTime;
		std::string volume = Utils::doubletostring(etrade->volume);
		std::string price = Utils::doubletostring(etrade->price);

		f << strategyname << "," << tradetime << "," << symbol << "," << direction << "," << offset << "," << price << "," << volume << "\n";

		f.close();
	}
}

void BacktestEngine::VarPlotRecord(StrategyTemplate* strategy_ptr, const BarData& bardata)
{


	std::unique_lock<std::mutex>lck(m_varplotrecordmtx);
	//�Ƚ��������ϲ���
	if (m_Vardata.m_strategy_varplotrecord_string.find(strategy_ptr->getparam("name")) == m_Vardata.m_strategy_varplotrecord_string.end())
	{
		std::map<std::string, std::vector<std::string>>tempmap;
		m_Vardata.m_strategy_varplotrecord_string[strategy_ptr->getparam("name")] = tempmap;
	}
	if (m_Vardata.m_strategy_varplotrecord_bool.find(strategy_ptr->getparam("name")) == m_Vardata.m_strategy_varplotrecord_bool.end())
	{
		std::map<std::string, std::vector<bool>>tempmap;
		m_Vardata.m_strategy_varplotrecord_bool[strategy_ptr->getparam("name")] = tempmap;
	}
	if (m_Vardata.m_strategy_varplotrecord_mainchart.find(strategy_ptr->getparam("name")) == m_Vardata.m_strategy_varplotrecord_mainchart.end())
	{
		std::map<std::string, std::vector<double>>tempmap;
		m_Vardata.m_strategy_varplotrecord_mainchart[strategy_ptr->getparam("name")] = tempmap;
	}
	if (m_Vardata.m_strategy_varplotrecord_indicator.find(strategy_ptr->getparam("name")) == m_Vardata.m_strategy_varplotrecord_indicator.end())
	{
		std::map<std::string, std::vector<double>>tempmap;
		m_Vardata.m_strategy_varplotrecord_indicator[strategy_ptr->getparam("name")] = tempmap;
	}


	if (m_Vardata.m_strategy_bardata.find(strategy_ptr->getparam("name")) == m_Vardata.m_strategy_bardata.end())
	{
		std::vector<BarData>v;
		v.push_back(bardata);
		m_Vardata.m_strategy_bardata[strategy_ptr->getparam("name")] = v;
	}
	else
	{
		m_Vardata.m_strategy_bardata[strategy_ptr->getparam("name")].push_back(bardata);
	}

	//���ʲ��Ե�
	std::map<std::string, std::string>map;
	map = strategy_ptr->GetVarPlotMap();

	std::map<std::string, std::string>indicator = strategy_ptr->GetIndicatorMap();

	//����

	for (std::map<std::string, std::string>::iterator iter = map.begin(); iter != map.end(); iter++)
	{
		if (iter->second == "true" || iter->second == "false")
		{
			//����ǲ�����ת���ɲ�����
			if (m_Vardata.m_strategy_varplotrecord_bool[strategy_ptr->getparam("name")].find(iter->first) == m_Vardata.m_strategy_varplotrecord_bool[strategy_ptr->getparam("name")].end())
			{
				std::vector<bool>v;
				v.push_back(Utils::stringtobool(iter->second));
				m_Vardata.m_strategy_varplotrecord_bool[strategy_ptr->getparam("name")].insert(std::pair<std::string, std::vector<bool>>(iter->first, v));
			}
			else
			{
				m_Vardata.m_strategy_varplotrecord_bool[strategy_ptr->getparam("name")][iter->first].push_back(Utils::stringtobool(iter->second));
			}
		}
		else if (Utils::isnum(iter->second))
		{
			//�������ֵ��
			if (m_Vardata.m_strategy_varplotrecord_mainchart[strategy_ptr->getparam("name")].find(iter->first) == m_Vardata.m_strategy_varplotrecord_mainchart[strategy_ptr->getparam("name")].end())
			{
				std::vector<double>v;
				v.push_back(atof(iter->second.c_str()));
				m_Vardata.m_strategy_varplotrecord_mainchart[strategy_ptr->getparam("name")].insert(std::pair<std::string, std::vector<double>>(iter->first, v));
			}
			else
			{
				m_Vardata.m_strategy_varplotrecord_mainchart[strategy_ptr->getparam("name")][iter->first].push_back(atof(iter->second.c_str()));
			}
		}
		else
		{
			//������ַ���

			if (m_Vardata.m_strategy_varplotrecord_string[strategy_ptr->getparam("name")].find(iter->first) == m_Vardata.m_strategy_varplotrecord_string[strategy_ptr->getparam("name")].end())
			{
				std::vector<std::string>v;
				v.push_back(iter->second);
				m_Vardata.m_strategy_varplotrecord_string[strategy_ptr->getparam("name")].insert(std::pair<std::string, std::vector<std::string>>(iter->first, v));
			}
			else
			{
				m_Vardata.m_strategy_varplotrecord_string[strategy_ptr->getparam("name")][iter->first].push_back(iter->second);
			}
		}
	}

	for (std::map<std::string, std::string>::iterator iter = indicator.begin(); iter != indicator.end(); iter++)
	{
		//�������ֵ��
		if (m_Vardata.m_strategy_varplotrecord_indicator[strategy_ptr->getparam("name")].find(iter->first) == m_Vardata.m_strategy_varplotrecord_indicator[strategy_ptr->getparam("name")].end())
		{
			std::vector<double>v;
			v.push_back(atof(iter->second.c_str()));
			m_Vardata.m_strategy_varplotrecord_indicator[strategy_ptr->getparam("name")].insert(std::pair<std::string, std::vector<double>>(iter->first, v));
		}
		else
		{
			m_Vardata.m_strategy_varplotrecord_indicator[strategy_ptr->getparam("name")][iter->first].push_back(atof(iter->second.c_str()));
		}
	}
}