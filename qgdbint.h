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
	void run(QString input); // havn't select-target
	void exec(QString cmd); // no '\n' needed
	void terminate();

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

	void start(QString program, QStringList arguments = QStringList(), QString input = "");
	void cont();
	void exit();
	int setBreakpoint(int row);
	int setBreakpoint(QString func, int* row = nullptr);
	void delBreakpoint(int id);
	void delAllBreakpoints();
	void disableBreakpoint(int id);
	void enableBreakpoint(int id);
	void step();
	void stepIn();
	void stepOut();
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
};

}
