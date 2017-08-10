#ifndef JSBACKTEST_H
#define JSBACKTEST_H

#include <QtWidgets/QWidget>
#include <qobject.h>
#include <qgroupbox.h>
#include <qcombobox.h>
#include <qgridlayout.h>
#include <qtextbrowser.h>
#include <qpushbutton.h>
#include <qprogressbar.h>
#include <qdatetimeedit.h>
#include <qstandarditemmodel.h>
#include <qtableview.h>
#include <qthread.h>
#include <qradiobutton.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <qcheckbox.h>
#include <qthread.h>
#include <QtWebEngineWidgets/qwebengineview.h>
#include <QtWebChannel/qwebchannel.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsondocument.h>
#include "ui_jsbacktest.h"
#include "backtestengine.h"
#define BAR_MODE "barmode"
#define TICK_MODE "tickmode"
class TransferObject :public QObject		//��������������ݵ�Javascript��
{
	Q_OBJECT

public:
	void setSendCapital(const QString &text);
	void setSendVar(const QString &text);
	public slots :
	void finished(bool success);
signals:
	void sendCapital(const QString & text);
	void sendVar(const QString & text);
private:
	bool m_success = false;
	QString m_capital = "";
	QString m_var = "";
};

class ResultPoint
{
public:
	std::string X;//ʱ��
	std::string Y;//��ֵ
};

class JSBacktest : public QWidget
{
	Q_OBJECT
		signals :
	void runbacktest();
	void stopbacktest();
public:
	JSBacktest(QWidget *parent = 0);
	~JSBacktest();

private:
	Ui::JSBacktestClass ui;
	void ClearMemorys();

	QString str2qstr(const std::string str);
	void CreateMainGUI();
	QGroupBox *createcontrolGroup();
	QGroupBox *createtableviewGroup();
	QGroupBox *createlogGroup();
	void CreateBacktestEngine();
	void ConnectSignalsSlots();
	void CreatePlotWidget();
	//����ؼ�
	QCheckBox *VarPlotCheckBox;

	QComboBox *strategybox;
	QTableView *tableview;
	QStandardItemModel* m_Model;
	QDateTimeEdit *start;
	QDateTimeEdit *end;
	QRadioButton *TickButton;
	QRadioButton *BarButton;
	QTextBrowser *textbrowser;
	QProgressBar *progressbar;
	//*************
	//�ز�����
	BacktestEngine * m_backtestengine;
	//�ز��߳�
	QThread *m_thread;

	//�ز⾻ֵ����
	QWidget *CapitalCurveWidget;
	TransferObject m_transerobject;
	std::mutex m_checkbox_ResultVectormtx;
	std::map<QCheckBox*, std::vector<ResultPoint>>m_checkbox_ResultVector;//ÿһ�����Զ�Ӧ�ľ�ֵ����
	std::vector<QCheckBox*>m_checkbox_v;  //װ����checkbox
	QVBoxLayout *strategylayout;		//װ���в��Ե�checkbox
	//�ز�����
	std::map<std::string, QWebEngineView*> m_vardata_view;//װ�����в��Զ�Ӧ�Ļز�����ͼ��
	std::vector<TransferObject*> m_vardata_transferpointer;

	private slots:
	void add_item_combobox(QString strategyname);
	void add_symbol_strategy();
	void del_symbol_strategy();
	void start_backtest();
	void stop_backtest();
	void plotcurve(PLOTDATA plotdata);
	void checkedcurve();
	void setrange(int max);
	void setvalueSLOT(int value);
	void plotVarSlots(VarData vardata);
};

#endif // JSBACKTEST_H
