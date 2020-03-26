#include <QProcess>
#include <functional>

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
	void ReadyStdout(QString output);
	void ReadyStderr(QString output);
	void ErrorOccurred(QProcess::ProcessError err);

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

	void waitUntilPause();

	void start(QString program, QStringList arguments = QStringList(), QString input = "");
	bool connect();
	void cont();
	void exit();

signals:
	void stateChanged(bool running, QString reason, QGdb* self);
	void textResponse(QString text, QGdb* self);
	void exited(QGdb* self);

private slots:
	void onRecord(QStringList record);

private:
	static QStringList filter(QStringList record, QChar head);
	static QString unescape(QString str, QString* rest = nullptr);
	static QString pickResult(QString& row); // take the result before ',' and set row to the rest of it
	static QMap<QString, QString> parseKeyStringPair(QString row);

	QGdbProcessManager* manager;
	QString gdb, gdbServer;
	int port;
	bool state; // whether running
	std::function<void(QStringList)> reqHandle, defHandle;
};

}
