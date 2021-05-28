#pragma once

#include <QProcess>
#include <functional>
#include "record.h"

namespace qgdbint {

class QGdbProcessManager : public QObject {
	Q_OBJECT

public:
	using QObject::QObject;
	~QGdbProcessManager();
	void prepare(QString program, QStringList arguments, QString gdbPath, QString gdbServerPath, int port = 9513);
	void clear();

public slots:
	void run(); // havn't select-target
	void exec(QString cmd); // no '\n' needed
	void terminate();
	void input(QString buffer);
	void inputFin();

signals:
	void Record(QStringList record);
	void readyStdout(QString output);
	void readyStderr(QString output);
	void errorOccurred(QProcess::ProcessError err);

private slots:
	void onReadyRead();
	void onDispatchStdout();
	void onDispatchStderr();
	void onFinished();

private:
	QProcess* gdb;
	QProcess* gdbServer;
	QByteArray buffer;
};
class QGdb : public QObject {
	Q_OBJECT

public:
	QGdb(QString gdbPath, QString gdbServerPath, int port = 12345, QObject* parent = nullptr);

	QString waitUntilPause();

	void input(QString buffer, bool finish = false);
	void finishInput();

	void start(QString program, QStringList arguments = QStringList());

	// sync
	void exit();
	int setBreakpoint(int row);
	int setBreakpoint(QString func, int* row = nullptr);
	QString eval(QString expr);

	// async
	QString cont();
	QString delBreakpoint(int id);
	QString delAllBreakpoints();
	QString disableBreakpoint(int id);
	QString enableBreakpoint(int id);
	QString breakAfter(int id, int cnt);
	QString breakCondition(int id, QString cond);
	QString step();
	QString stepIn();
	QString stepOut();
	
	void autoWaitAsync(bool wait = true);

	void terminate();

signals:
	void stateChanged(bool running, QString reason, QGdb* self);
	void textResponse(QString text, QGdb* self);
	void exited(QGdb* self);
	void positionUpdated(QString file, int row, QGdb* self);
	void readyStdout(QString output);
	void readyStderr(QString output);
	void errorOccurered(QString message);

private slots:
	void onRecord(QStringList record);

private:
	static QStringList filter(QStringList record, QChar head);

	QGdbProcessManager* manager;
	QString gdb, gdbServer;
	int port;
	bool state; // whether running
	std::function<void(QStringList)> reqHandle, defHandle;
	bool wait;
};

}
